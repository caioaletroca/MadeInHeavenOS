#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <test.h>

extern char **environ; // POSIX: the program declares it, no header has to

/*
 * Lua, the first port (ports/lua/, roadmap D10f): /bin/lua runs each case's
 * chunk with -e and must exit with the status given. A chunk checks itself
 * with assert: a failed one is an uncaught error, which lua reports on stderr
 * ("lua: (command line):LINE: assertion failed!") and exits with 1. What each
 * case reaches below Lua: mlibc's math, float printing and parsing, setjmp
 * and longjmp (Lua's errors), malloc and free through mmap (the collector),
 * the clock, files on the test disk.
 *
 * Not here yet: io.popen and os.execute, which run "/bin/sh -c" and our shell
 * has no -c; os.rename, which the kernel lacks (checked to fail cleanly).
 */

#define LUA_PATH "/bin/lua"
#define LUAC_PATH "/bin/luac"

typedef struct lua_case
{
    const char *name;
    const char *chunk;
    int status;
} lua_case_t;

static const lua_case_t cases[] = {
    {"numbers and math",
     "assert(7 // 2 == 3 and 7 % 3 == 1 and 2^10 == 1024.0)\n"
     "assert(math.type(1) == 'integer' and math.type(1.0) == 'float')\n"
     "assert(math.maxinteger + 1 == math.mininteger)\n"
     "assert(math.sqrt(16) == 4 and math.floor(-2.5) == -3)\n"
     "assert(math.abs(math.sin(math.pi / 2) - 1) < 1e-12)\n"
     "assert(tostring(0.1) == '0.1' and tostring(1e100) == '1e+100')\n"
     "assert(tonumber('0x10') == 16 and tonumber('1.5e3') == 1500.0)\n"
     "assert(tonumber('  12  ') == 12 and tonumber('12a') == nil)\n"
     "assert(string.format('%5.2f|%d|%x', 3.14159, -42, 255) == ' 3.14|-42|ff')\n",
     0},
    {"strings and patterns",
     "assert(('abc'):upper() == 'ABC' and ('ab'):rep(3, '-') == 'ab-ab-ab')\n"
     "assert(select(2, ('a1b22c333'):gsub('%d+', '#')) == 3)\n"
     "assert(('key = value'):match('^(%w+)%s*=%s*(%w+)$') == 'key')\n"
     "assert(utf8.char(227) == '\\xC3\\xA3' and utf8.len('a\\xC3\\xA3b') == 3)\n"
     "assert(table.concat({1, 2, 3}, ',') == '1,2,3')\n",
     0},
    {"tables, closures and metatables",
     "local t = {5, 3, 9, 1}\n"
     "table.sort(t)\n"
     "assert(t[1] == 1 and t[4] == 9 and #t == 4)\n"
     "local function counter() local n = 0 return function() n = n + 1 return n end end\n"
     "local c = counter() c() assert(c() == 2)\n"
     "local V = {} V.__index = V V.__add = function(a, b) return setmetatable({x = a.x + b.x}, V) end\n"
     "local sum = setmetatable({x = 1}, V) + setmetatable({x = 2}, V)\n"
     "assert(sum.x == 3 and getmetatable(sum) == V)\n",
     0},
    {"errors and coroutines",
     "local ok, e = pcall(error, {code = 7})\n"
     "assert(not ok and e.code == 7)\n"
     "ok, e = pcall(function() return 1 + {} end)\n"
     "assert(not ok and e:find('arithmetic'))\n"
     "ok, e = xpcall(function() error('deep') end, debug.traceback)\n"
     "assert(not ok and e:find('stack traceback'))\n"
     "local gen = coroutine.wrap(function() for i = 1, 3 do coroutine.yield(i) end end)\n"
     "assert(gen() == 1 and gen() == 2 and gen() == 3)\n",
     0},
    {"the garbage collector gives memory back",
     "local keep = {}\n"
     "for i = 1, 200000 do keep[i] = {i} end\n"
     "local full = collectgarbage('count')\n"
     "keep = nil\n"
     "collectgarbage('collect')\n"
     "assert(collectgarbage('count') < full / 4)\n",
     0},
    {"time",
     "assert(os.date('!%Y-%m-%d %H:%M:%S', 0) == '1970-01-01 00:00:00')\n"
     "assert(os.time({year = 2000, month = 1, day = 1, hour = 0}) == 946684800)\n"
     "assert(os.time() > 946684800 and os.clock() >= 0)\n"
     "assert(os.difftime(10, 4) == 6.0)\n",
     0},
    // The test disk on /mnt is writable in both boots
    {"files",
     "local name = '/mnt/lua.txt'\n"
     "local f = assert(io.open(name, 'w'))\n"
     "f:write('one\\n', 2, '\\n', 3.5, '\\n') f:close()\n"
     "local lines = {} for line in io.lines(name) do lines[#lines + 1] = line end\n"
     "assert(#lines == 3 and lines[1] == 'one' and lines[2] == '2' and lines[3] == '3.5')\n"
     "f = assert(io.open(name, 'r'))\n"
     "assert(f:seek('end') == 10 and f:seek('set', 4) == 4 and f:read('n') == 2)\n"
     "f:close()\n"
     "assert(os.remove(name))\n"
     "assert(io.open(name, 'r') == nil)\n",
     0},
    {"missing calls fail cleanly",
     "assert(os.rename('/no/such', '/no/other') == nil)\n"
     "local root = os.getenv('TEST_ROOT')\n"
     "assert(root == 'disk' or root == 'initrd')\n",
     0},
    // stderr goes to /dev/null for this one: the message is expected
    {"an uncaught error exits with 1", "error('expected')\n", 1},
    {NULL, NULL, 0},
};

// Runs program with argv, stderr to /dev/null when quiet; returns the exit status or -1
static int run(char *const argv[], int quiet)
{
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    if (quiet)
        posix_spawn_file_actions_addopen(&actions, 2, "/dev/null", O_WRONLY, 0);

    pid_t pid;
    int error = posix_spawn(&pid, argv[0], &actions, NULL, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    if (error != 0)
    {
        printf("could not start %s: %s\n", argv[0], strerror(error));
        return -1;
    }

    int status;
    if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status))
        return -1;
    return WEXITSTATUS(status);
}

int main(void)
{
    for (const lua_case_t *c = cases; c->name != NULL; c++)
    {
        char *argv[] = {LUA_PATH, "-e", (char *)c->chunk, NULL};
        int status = run(argv, c->status != 0);
        if (status != c->status)
            printf("case \"%s\": status %d, expected %d\n", c->name, status, c->status);
        CHECK(status == c->status);
    }

    // The compiler: prints its version and exits with 0
    char *luac_argv[] = {LUAC_PATH, "-v", NULL};
    CHECK(run(luac_argv, 0) == 0);

    return test_status();
}
