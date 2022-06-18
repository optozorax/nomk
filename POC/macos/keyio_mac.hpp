#pragma once

#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hidsystem/IOHIDShared.h>
#include <unistd.h>
#include <errno.h>
#include <map>
#include <iostream>
#include <mach/mach_error.h>

/*
 * Key event information that's shared between C++ and Haskell.
 *
 * type: represents key up or key down
 * page: represents IOKit usage page
 * usage: represents IOKit usage
 */
struct KeyEvent {
    uint64_t type;
    uint32_t page;
    uint32_t usage;
};

/*
 * These are needed to receive unaltered key events from the OS.
 */
extern std::map<io_service_t,IOHIDDeviceRef> source_device;

void print_iokit_error(const char *fname, int freturn = 1);

/*
 * We'll register this callback to run whenever an IOHIDDevice
 * (representing a keyboard) sends input from the user.
 *
 * It passes the relevant information into a pipe that will be read
 * from with wait_key.
 */
void input_callback(void *context, IOReturn result, void *sender, IOHIDValueRef value);

void open_matching_devices(char *product, io_iterator_t iter);

/*
 * We'll register this callback to run whenever an IOHIDDevice
 * (representing a keyboard) is connected to the OS
 *
 */
void matched_callback(void *context, io_iterator_t iter);

/*
 * We'll register this callback to run whenever an IOHIDDevice
 * (representing a keyboard) is disconnected from the OS
 *
 */
void terminated_callback(void *context, io_iterator_t iter);

/*
 * For each keyboard, registers an asynchronous callback to run when
 * new input from the user is available from that keyboard. Then
 * sleeps indefinitely, ready to received asynchronous callbacks.
 */
void monitor_kb(char *product);
