#include "keyio_mac.hpp"

/*
 * These are needed to receive unaltered key events from the OS.
 */
struct Context {
    char* product;
    CFRunLoopRef listener_loop;
    std::map<io_service_t, IOHIDDeviceRef> source_device;
};

void print_iokit_error(const char *fname, int freturn) {
    std::cerr << fname << " error";
    if (freturn) {
        std::cerr << " " << std::hex << freturn;
        std::cerr << ": ";
        std::cerr << mach_error_string(freturn);
    }
    std::cerr << std::endl;
}

/*
 * We'll register this callback to run whenever an IOHIDDevice
 * (representing a keyboard) sends input from the user.
 *
 * It passes the relevant information into a pipe that will be read
 * from with wait_key.
 */
void input_callback(void *context, IOReturn result, void *sender, IOHIDValueRef value) {
    struct KeyEvent e;
    IOHIDElementRef element = IOHIDValueGetElement(value);
    e.type = IOHIDValueGetIntegerValue(value);
    e.page = IOHIDElementGetUsagePage(element);
    e.usage = IOHIDElementGetUsage(element);
    #define print(a) << "\n  " << #a << ": " << a(element) << ", "
    #define print_bool(a) << "\n  " << #a << ": " << (a(element) ? "true" : "false") << ", "
    std::cout  << "---------- ELEMENT EVENT!!!!!! -------"
        << "\n  type: " << IOHIDValueGetIntegerValue(value) << ", "
        << "\n  page: " << IOHIDElementGetUsagePage(element) << ", "
        << "\n  usage: " << IOHIDElementGetUsage(element) << ", "
        print(IOHIDElementGetDevice)
        // print(IOHIDElementGetType)
        // print_bool(IOHIDElementIsVirtual)
        // print_bool(IOHIDElementIsRelative)
        // print_bool(IOHIDElementIsWrapping)
        // print_bool(IOHIDElementIsArray)
        // print_bool(IOHIDElementIsNonLinear)
        // print(IOHIDElementGetName)
        print(IOHIDElementGetReportID)
        print(IOHIDElementGetReportSize)
        // print(IOHIDElementGetReportCount)
        // print(IOHIDElementGetUnit)
        // print(IOHIDElementGetUnitExponent)
        << "\n\n" << std::flush;
}

void open_matching_devices(Context* context, io_iterator_t iter) {
    io_name_t name;
    kern_return_t kr;
    CFStringRef cfproduct = NULL;
    if(context->product) {
        cfproduct = CFStringCreateWithCString(kCFAllocatorDefault, context->product, CFStringGetSystemEncoding());
        if(cfproduct == NULL) {
            print_iokit_error("CFStringCreateWithCString");
            return;
        }
    }
    CFStringRef cfkarabiner = CFStringCreateWithCString(kCFAllocatorDefault, "Karabiner VirtualHIDKeyboard", CFStringGetSystemEncoding());
    if (cfkarabiner == NULL) {
        print_iokit_error("CFStringCreateWithCString");
        if (context->product) {
            CFRelease(cfproduct);
        }
        return;
    }
    for (mach_port_t curr = IOIteratorNext(iter); curr; curr = IOIteratorNext(iter)) {
        CFStringRef cfcurr = (CFStringRef)IORegistryEntryCreateCFProperty(curr, CFSTR(kIOHIDProductKey), kCFAllocatorDefault, kIOHIDOptionsTypeNone);
        if(cfcurr == NULL) {
            print_iokit_error("IORegistryEntryCreateCFProperty");
            continue;
        }
        bool match = (CFStringCompare(cfcurr, cfkarabiner, 0) != kCFCompareEqualTo);
        if(context->product) {
            match = match && (CFStringCompare(cfcurr, cfproduct, 0) == kCFCompareEqualTo);
        }
        CFRelease(cfcurr);
        if(!match) continue;
        IOHIDDeviceRef dev = IOHIDDeviceCreate(kCFAllocatorDefault, curr);
        context->source_device[curr] = dev;
        IOHIDDeviceRegisterInputValueCallback(dev, input_callback, NULL);
        kr = IOHIDDeviceOpen(dev, kIOHIDOptionsTypeSeizeDevice);
        if(kr != kIOReturnSuccess) {
            print_iokit_error("IOHIDDeviceOpen", kr);
        }
        IOHIDDeviceScheduleWithRunLoop(dev, context->listener_loop, kCFRunLoopDefaultMode);
    }
    if(context->product) {
        CFRelease(cfproduct);
    }
    CFRelease(cfkarabiner);
}

/*
 * We'll register this callback to run whenever an IOHIDDevice
 * (representing a keyboard) is connected to the OS
 */
void matched_callback(void *context, io_iterator_t iter) {
    std::cout << "Keyboard plugged" << std::endl;
    Context* context2 = (Context*)context;
    open_matching_devices(context2, iter);
}

/*
 * We'll register this callback to run whenever an IOHIDDevice
 * (representing a keyboard) is disconnected from the OS
 */
void terminated_callback(void *context, io_iterator_t iter) {
    std::cout << "Keyboard unplugged" << std::endl;
    Context* context2 = (Context*)context;
    for (mach_port_t curr = IOIteratorNext(iter); curr; curr = IOIteratorNext(iter)) {
        context2->source_device.erase(curr);
    }
}

/*
 * For each keyboard, registers an asynchronous callback to run when
 * new input from the user is available from that keyboard. Then
 * sleeps indefinitely, ready to received asynchronous callbacks.
 */
void monitor_kb(char *product) {
    kern_return_t kr;
    CFMutableDictionaryRef matching_dictionary = IOServiceMatching(kIOHIDDeviceKey);
    if (!matching_dictionary) {
        print_iokit_error("IOServiceMatching");
        return;
    }
    UInt32 value;
    CFNumberRef cfValue;
    value = kHIDPage_GenericDesktop;
    cfValue = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value);
    CFDictionarySetValue(matching_dictionary, CFSTR(kIOHIDDeviceUsagePageKey), cfValue);
    CFRelease(cfValue);
    value = kHIDUsage_GD_Keyboard;
    cfValue = CFNumberCreate( kCFAllocatorDefault, kCFNumberSInt32Type, &value );
    CFDictionarySetValue(matching_dictionary,CFSTR(kIOHIDDeviceUsageKey),cfValue);
    CFRelease(cfValue);
    io_iterator_t iter = IO_OBJECT_NULL;

    Context context = {
        .product = product,
        .listener_loop = CFRunLoopGetCurrent(),
        .source_device = {}
    };
    IONotificationPortRef notification_port = IONotificationPortCreate(kIOMainPortDefault);
    CFRunLoopSourceRef notification_source = IONotificationPortGetRunLoopSource(notification_port);
    CFRunLoopAddSource(context.listener_loop, notification_source, kCFRunLoopDefaultMode);
    CFRetain(matching_dictionary);
    kr = IOServiceAddMatchingNotification(notification_port,
                                          kIOMatchedNotification,
                                          matching_dictionary,
                                          matched_callback,
                                          &context,
                                          &iter);
    if (kr != KERN_SUCCESS) {
        print_iokit_error("IOServiceAddMatchingNotification", kr);
        return;
    }
    open_matching_devices(&context, iter);
    kr = IOServiceAddMatchingNotification(notification_port,
                                          kIOTerminatedNotification,
                                          matching_dictionary,
                                          terminated_callback,
                                          &context,
                                          &iter);
    if (kr != KERN_SUCCESS) {
        print_iokit_error("IOServiceAddMatchingNotification", kr);
        return;
    }
    for (mach_port_t curr = IOIteratorNext(iter); curr; curr = IOIteratorNext(iter)) {}
    CFRunLoopRun();
    for(std::pair<const io_service_t, IOHIDDeviceRef> p : context.source_device) {
        kr = IOHIDDeviceClose(p.second,kIOHIDOptionsTypeSeizeDevice);
        if(kr != KERN_SUCCESS) {
            print_iokit_error("IOHIDDeviceClose", kr);
        }
    }
}
