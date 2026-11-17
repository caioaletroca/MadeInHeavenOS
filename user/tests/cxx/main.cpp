#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <algorithm>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <vector>
#include <test.h>

/*
 * C++ on libstdc++ and mlibc (roadmap D10e). The language's runtime: global
 * constructors before main (in order), atexit handlers and then global
 * destructors at exit (each in reverse order of registration), operator
 * new/delete, virtual calls, local statics initialized once, exceptions
 * (unwinding through destructors), RTTI, thread_local. Then the library:
 * containers, strings, string streams, algorithms, smart pointers.
 *
 * The exit status proves the destructors ran: the last one ends the process
 * with 42 (1 if any check failed), so missing destructors leave main's 0.
 */

#define DESTRUCTORS_RAN 42

static int order[2];
static int constructed;
static int destroyed;

struct Recorder
{
    int id;

    explicit Recorder(int id) : id(id)
    {
        if (constructed < 2)
            order[constructed] = id;
        constructed++;
    }

    ~Recorder()
    {
        destroyed++;

        // Reverse order of construction: 2 first, then 1
        if (id != 3 - destroyed)
        {
            test_failures++;
            printf("FAIL: destructor %d ran as number %d\n", id, destroyed);
        }

        if (destroyed == 2)
            _exit(test_failures == 0 ? DESTRUCTORS_RAN : 1);
    }
};

static Recorder first(1);
static Recorder second(2);

// Registered in main, after the constructors registered the destructors: runs before them
static void before_destructors(void)
{
    CHECK(destroyed == 0);
}

static int instance_calls;

struct Counted
{
    Counted() { instance_calls++; }
};

static Counted &instance()
{
    static Counted counted; // constructed on the first call only
    return counted;
}

struct Shape
{
    virtual ~Shape() = default;
    virtual int area() const = 0;
};

struct Square : Shape
{
    int side;
    explicit Square(int side) : side(side) {}
    int area() const override { return side * side; }
};

struct Rectangle : Shape
{
    int width, height;
    Rectangle(int width, int height) : width(width), height(height) {}
    int area() const override { return width * height; }
};

static int alive;

struct Tracked
{
    Tracked() { alive++; }
    ~Tracked() { alive--; }
};

static void test_runtime(void)
{
    CHECK(constructed == 2);
    CHECK(order[0] == 1 && order[1] == 2);
    CHECK(atexit(before_destructors) == 0);

    CHECK(&instance() == &instance());
    CHECK(instance_calls == 1);

    // Virtual calls through the base, deleted through its virtual destructor (sized delete)
    Shape *shapes[] = {new Square(3), new Rectangle(2, 5)};
    CHECK(shapes[0]->area() == 9);
    CHECK(shapes[1]->area() == 10);

    // RTTI: dynamic_cast checks the real type, typeid names it
    CHECK(dynamic_cast<Square *>(shapes[0]) != nullptr);
    CHECK(dynamic_cast<Square *>(shapes[1]) == nullptr);
    CHECK(typeid(*shapes[1]) == typeid(Rectangle));
    CHECK(typeid(*shapes[0]) != typeid(*shapes[1]));
    for (Shape *shape : shapes)
        delete shape;

    // new aligns for any object of the size asked (C++17): mlibc gives an int
    // 8 bytes (glibc 16), but a 16-byte long double must be 16-aligned
    int *number = new int(7);
    CHECK(*number == 7);
    CHECK((reinterpret_cast<uintptr_t>(number) % alignof(int)) == 0);
    delete number;

    long double *wide = new long double(1.5L);
    CHECK(*wide == 1.5L);
    CHECK((reinterpret_cast<uintptr_t>(wide) % alignof(long double)) == 0);
    delete wide;

    // Arrays of objects with destructors: new[] stores the count, delete[] runs every destructor
    Tracked *many = new Tracked[10];
    CHECK(alive == 10);
    delete[] many;
    CHECK(alive == 0);
}

static void throw_with_locals(int depth)
{
    Tracked local;
    if (depth == 0)
        throw std::runtime_error("deep");
    throw_with_locals(depth - 1);
}

static void test_exceptions(void)
{
    // Thrown, caught by a base class, its message kept
    int caught = 0;
    try
    {
        throw std::out_of_range("range");
    }
    catch (const std::logic_error &error)
    {
        caught = std::string(error.what()) == "range";
    }
    CHECK(caught);

    // Unwinding runs the destructors of every frame it leaves
    caught = 0;
    try
    {
        throw_with_locals(5);
    }
    catch (const std::runtime_error &)
    {
        caught = alive == 0;
    }
    CHECK(caught);

    // Rethrown from a catch-all, caught again by its type
    caught = 0;
    try
    {
        try
        {
            throw 7;
        }
        catch (...)
        {
            throw;
        }
    }
    catch (int value)
    {
        caught = value == 7;
    }
    CHECK(caught);

    // A refused allocation throws std::bad_alloc: malloc refuses above PTRDIFF_MAX
    volatile size_t huge = (size_t)PTRDIFF_MAX + 1;
    caught = 0;
    try
    {
        void *block = ::operator new(huge);
        ::operator delete(block);
    }
    catch (const std::bad_alloc &)
    {
        caught = 1;
    }
    CHECK(caught);

    // The library's own checks throw too
    std::vector<int> empty;
    caught = 0;
    try
    {
        (void)empty.at(3);
    }
    catch (const std::out_of_range &)
    {
        caught = 1;
    }
    CHECK(caught);
}

static thread_local int per_thread = 5;

static void test_library(void)
{
    std::vector<int> numbers;
    for (int i = 0; i < 1000; i++)
        numbers.push_back(999 - i);
    std::sort(numbers.begin(), numbers.end());
    CHECK(numbers.size() == 1000 && numbers.front() == 0 && numbers.back() == 999);
    CHECK(std::is_sorted(numbers.begin(), numbers.end()));

    std::string text = "made";
    text += " in ";
    text.append(std::string(3, '!'));
    CHECK(text == "made in !!!");
    CHECK(text.find("in") == 5);
    CHECK(std::stoi("-42") == -42);
    CHECK(std::to_string(1234) == "1234");

    std::map<std::string, int> counts;
    for (const char *word : {"b", "a", "b", "c", "b"})
        counts[word]++;
    CHECK(counts.size() == 3 && counts["b"] == 3);
    CHECK(counts.begin()->first == "a"); // ordered by key

    std::ostringstream out;
    out << 42 << ' ' << 1.5 << ' ' << std::hex << 255;
    CHECK(out.str() == "42 1.5 ff");

    std::istringstream in("7 8.25 word");
    int whole = 0;
    double fraction = 0;
    std::string word;
    in >> whole >> fraction >> word;
    CHECK(whole == 7 && fraction == 8.25 && word == "word");

    {
        auto shared = std::make_shared<Tracked>();
        auto copy = shared;
        CHECK(alive == 1 && shared.use_count() == 2);
        std::unique_ptr<Tracked> owned(new Tracked);
        CHECK(alive == 2);
    }
    CHECK(alive == 0);

    int base = 10;
    std::function<int(int)> add = [base](int value) { return base + value; };
    CHECK(add(5) == 15);

    CHECK(per_thread == 5); // the thread block's initial image
    per_thread++;
    CHECK(per_thread == 6);
}

int main()
{
    test_runtime();
    test_exceptions();
    test_library();
    return test_status();
}
