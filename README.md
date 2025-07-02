# STM32F401 Real-Time Kernel

A comprehensive real-time operating system (RTOS) kernel implementation for STM32F401xB/C and STM32F401xD/E microcontrollers, featuring advanced ARM Cortex-M4 based 32-bit MCU support with real-time thread scheduling, peripheral drivers, and communication protocols.

## 🚀 Overview

This project implements a fully functional real-time kernel designed for embedded systems applications. Developed as part of CMU's 18-349 Introduction to Embedded Systems course

### Key Features

- **Real-Time Thread Scheduling**: Priority-based preemptive scheduler with Rate Monotonic Scheduling (RMS)
- **Immediate Priority Ceiling Protocol (IPCP)**: Advanced mutex implementation preventing priority inversion
- **Hardware Abstraction Layer**: Complete driver support for STM32F401 peripherals
- **Communication Protocols**: UART and I2C implementations
- **Peripheral Drivers**: LCD display, keypad, GPIO, and timer support
- **Memory Protection Unit (MPU)**: Process isolation and memory management
- **System Call Interface**: Secure kernel-user mode transitions

## 🏗️ Architecture

### Core Components

```
📦 Kernel Architecture
├── 🧠 Scheduling System
│   ├── Thread Control Blocks (TCB)
│   ├── Priority-based scheduler
│   ├── Context switching (PendSV)
│   └── SysTick timer integration
├── 🔒 Synchronization
│   ├── IPCP mutex implementation
│   ├── Priority inheritance
│   └── Deadlock prevention
├── 💾 Memory Management
│   ├── Stack allocation
│   ├── MPU configuration
│   └── User/Kernel space separation
└── 🔌 Hardware Abstraction
    ├── GPIO driver
    ├── UART communication
    ├── I2C protocol
    ├── Timer management
    └── Interrupt handling
```

### Thread States

- `NEW`: Thread created but not yet scheduled
- `READY`: Thread ready for execution
- `RUNNING`: Currently executing thread
- `WAITING`: Thread waiting for next period
- `BLOCKED`: Thread blocked on mutex
- `DONE`: Thread completed execution

## 🛠️ Hardware Support

### Target Microcontroller
- **STM32F401xB/C**: 256KB Flash, 64KB SRAM
- **STM32F401xD/E**: 512KB Flash, 96KB SRAM
- **ARM Cortex-M4**: 32-bit RISC processor
- **Clock Speed**: Up to 84 MHz
- **FPU**: Single-precision floating-point unit

### Supported Peripherals
- **GPIO**: General-purpose I/O with configurable modes
- **UART**: Serial communication with configurable baud rates
- **I2C**: Two-wire interface for sensor communication
- **SysTick**: System timer for scheduling
- **MPU**: Memory protection and isolation
- **Timers**: General-purpose timers for precise timing
- **LCD Display**: Character-based display driver
- **Keypad**: Matrix keypad input driver



## 🔧 Building and Deployment

### Prerequisites

- **ARM GCC Toolchain**: `arm-none-eabi-gcc`
- **OpenOCD**: For debugging and flashing
- **Make**: Build system
- **STM32 Development Board**: STM32F401RE Nucleo recommended

### Build Instructions

1. **Clone the repository**:
   ```bash
   git clone <repository-url>
   cd lab-4-real-time-kernel-macuz_yacil_kernel
   ```

2. **Build the kernel**:
   ```bash
   make clean
   make
   ```

3. **Build specific test**:
   ```bash
   make USER_PROJ=test_1_0
   ```

4. **Flash to device**:
   ```bash
   # For Windows
   windows_ocd.bat
   
   # For Linux
   ./linux_ocd
   
   # For macOS
   ./osx_ocd
   ```

### Build Options

- `USER_PROJ`: Select test project (default: `default`)
- `DEBUG`: Enable debug symbols (default: `1`)
- `FLOAT`: Floating-point support (`soft`/`hard`)

## 📊 Scheduling Algorithm

### Rate Monotonic Scheduling (RMS)
The kernel implements RMS with the following characteristics:
- **Priority Assignment**: Higher frequency = Higher priority
- **Schedulability Test**: Utilization Bound (UB) test
- **Preemptive**: Higher priority threads preempt lower priority
- **Periodic**: Each thread has a defined period and computation time


### IPCP (Immediate Priority Ceiling Protocol)
- **Priority Ceiling**: Each mutex has a priority ceiling
- **Immediate Inheritance**: Thread inherits ceiling priority upon lock
- **Deadlock Prevention**: Prevents circular waiting
- **Bounded Blocking**: Guarantees bounded priority inversion



## 🧪 Testing

The project includes comprehensive test suites:

- **Basic Functionality**: Thread creation, scheduling, termination
- **Mutex Testing**: Priority inheritance, deadlock prevention
- **Timing Tests**: Real-time constraints, period enforcement
- **Stress Tests**: High-load scenarios, resource contention
- **Regression Tests**: Grading and validation scenarios



## 📚 Documentation

- **API Documentation**: Available in `doxygen_docs/index.html`
- **Hardware Reference**: STM32F401 datasheets in `docs/`
- **Code Comments**: Comprehensive inline documentation
- **Theory**: Real-time systems concepts and implementation details

## ⚠️ Limitations and Considerations

- **Maximum Threads**: Limited to 14 user threads + idle/default
- **Stack Size**: Power-of-2 sizes, maximum 32KB total
- **Mutex Count**: Up to 32 mutexes supported
- **Real-time Guarantees**: Subject to correct C and T parameters
- **Hardware Dependencies**: Optimized for STM32F401 family

## 🤝 Contributing

This project was developed for educational purposes. Key areas for extension:

- **Power Management**: Low-power modes and sleep states
- **Networking**: TCP/IP stack integration
- **File System**: Flash-based file system
- **Additional Drivers**: SPI, ADC, PWM support
- **Profiling Tools**: Execution time analysis


## 🔍 Additional Resources

- [STM32F401 Reference Manual](docs/m4_reference_manual.pdf)
- [ARM Cortex-M4 Programming Manual](docs/m4_programming_manual.pdf)
- [Real-Time Systems Theory](https://www.cs.cmu.edu/~raj/15-712/)
- [Embedded Systems Design Patterns](https://www.state-machine.com/)

