#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <test.h>

/*
 * The freestanding C++ runtime: global constructors before main (in order),
 * atexit handlers and then global destructors at exit (each in reverse order
 * of registration), operator new/delete on malloc, virtual calls, local
 * statics initialized once.
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

int main()
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
    for (Shape *shape : shapes)
        delete shape;

    int *number = new int(7);
    CHECK(*number == 7);
    CHECK((reinterpret_cast<uintptr_t>(number) & 15) == 0); // malloc's 16-byte alignment
    delete number;

    // Arrays of objects with destructors: new[] stores the count, delete[] runs every destructor
    Tracked *many = new Tracked[10];
    CHECK(alive == 10);
    delete[] many;
    CHECK(alive == 0);

    return test_status();
}
