# DAPLink Debugger

The DAPLink Debugger is an open-source debugger based on the ESP32S3 development board. It provides a comprehensive set of features for embedded systems development and debugging.

## Features

- **DAPLink Online Debugging**: Enables online debugging using the DAPLink interface, allowing for real-time code execution, breakpoints, and variable inspection.

- **CDC Serial Communication**: Supports CDC serial communication for seamless data transfer between the debugger and the target device.

- **Wireless Serial Logging**: Facilitates wireless serial logging, allowing developers to remotely monitor and analyze debug logs.

- **Offline Programming**: Provides the capability to perform offline programming by burning firmware onto the target device. This feature allows for firmware updates and device programming without the need for an active debugging session.

- **Automatic Keil FLM Algorithm Parsing**: Includes a built-in parser for automatically parsing Keil FLM algorithms. This feature enables the use of a wide range of Keil programming algorithms, making it compatible with various microcontrollers.

- **Customizable Programming Algorithms**: Offers the flexibility to customize programming algorithms based on Keil templates. Developers can create their own algorithms tailored to specific microcontrollers and compile them for offline programming.

- **Remote Automatic Programming**:  Integrates the offline programming feature with a web interface, enabling remote automatic programming. This functionality allows for remote firmware updates and device programming, making it ideal for scenarios where physical access to the target device is limited. Please note that the Remote Automatic Programming feature is currently under development and is not yet fully implemented. As the project creator, I am unable to complete this feature on my own due to my limited front-end development skills. If you have front-end development expertise and are interested in contributing to the project, your collaboration would be greatly appreciated.

- **Offline Programming porting**: The offline programming functionality has been separated from the main DAPLink code. If you want to implement your own offline programmer, you just need to implement the following interfaces in `swd_iface.cpp`:

```cpp
virtual void msleep(uint32t ms) = 0;
virtual bool init(void) = 0;
virtual bool off(void) = 0;
virtual transfer_err_def transer(uint32_t request, uint32_t data) = 0;
virtual void swj_sequence(uint32t count, const uint8_t data) = 0;
virtual void set_target_reset(uint8t asserted) = 0;
```

## Contribution

If you have front-end development skills or are interested in embedded systems development, you are welcome to contribute to the DAPLink Debugger project. Your contributions can help enhance existing features, add new functionalities, and improve the overall user experience.

Feel free to explore the project, contribute your ideas, and collaborate with the community. Together, we can make the DAPLink Debugger a powerful tool for embedded systems development and debugging.

Happy debugging!