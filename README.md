XOS :

XOS is a lightweight, educational operating system project created to help developers and students gain hands-on experience and better understand the inner workings of operating systems. It serves as a resource for anyone interested in OS principles and development, with a focus on simplicity, modularity, and extensive documentation.
Motivation

This project represents the culmination of my passion for operating systems and the desire to understand how an OS works from the ground up. The main motivation behind creating XOS was the realization that there are few small, accessible OS options available for learning and experimentation. While I have always been fascinated by how operating systems operate, I also wanted to make it easier for others to dive into the world of OS development.

I started this project out of sheer interest and love for OS development. However, over time, the project became a mission — it had to be completed! Despite having limited time, I worked on it primarily during weekends, nights, and holidays. It took a total of 1 year and 5 months to finish. The journey involved late nights (often until 4 AM) and weekends, but my passion, perseverance, and dedication drove me to push forward, learning valuable lessons along the way about focus, commitment, and patience.
Development Process
Time Commitment

Due to time constraints, I could only dedicate limited hours to this project, working on it after work, on weekends, and during holidays. Despite the long development period, I was fully committed to completing the OS, using every available moment to push the project forward.
Key Principles: Passion, Perseverance, and Learning

This project was driven by my passion for operating systems and my deep interest in how they function. Even when faced with challenges, perseverance was essential. I learned that resilience is key to completing long-term projects — it’s not always easy, but it’s worth it.
Features

    Lightweight Design: XOS is designed to be small, lightweight, and easy to understand. It's intended as a learning tool for anyone interested in operating systems.
    Extensive Comments: The code is heavily commented to provide insights into the development process, making it easier for others to follow along and learn.
    Modular Architecture: XOS is structured to allow easy addition of new features and components in the future.

Installation and Setup

To build and run XOS, follow the steps below.
Prerequisites:

    QEMU: An emulator for ARM64 architecture.
    Build Tools: Ensure you have make and other necessary build tools installed.

1. Install QEMU for ARM64

First, you need to install QEMU to emulate ARM64 architecture. You can install QEMU using the following commands based on your system:
On Ubuntu/Debian:

bash
sudo apt update
sudo apt install qemu qemu-system-arm

On macOS (using Homebrew):

bash
brew install qemu

2. Build XOS

Once you have QEMU installed, you can compile the XOS kernel using the provided build script.

bash
./build.sh make

This will compile the source code and generate the necessary files to run XOS.
3. Run XOS

After the build process completes successfully, you can run the operating system using QEMU with the following command:

bash
./run.sh run

This will start QEMU and launch XOS in the ARM64 emulation environment.

Although XOS is functional, I have several plans for future improvements and features, including:

    Process Management: Implementing multitasking and process scheduling.
    Multi-core Support: Enabling support for multi-core processors.
    Advanced Scheduling: Implementing priority scheduling and fair scheduling algorithms.
    Memory Management: Further improving memory management techniques.
    Device Driver Support: Adding support for more hardware devices.
    File System Improvements: Expanding file system functionality.
    Networking Support: Adding networking capabilities.

Contribution

We welcome contributions to the project! If you're interested in contributing, here’s how you can get started:

    Fork the repository: Create your own fork of the project.
    Create a new branch:

    bash
    git checkout -b feature/your-feature

    Commit your changes:

    bash
    git commit -am 'Add new feature'

    Push your branch:

    bash
    git push origin feature/your-feature

    Create a Pull Request: Open a pull request with a description of the changes.

Please ensure that your code follows the project’s coding style and contribution guidelines. This helps maintain consistency and quality.
Conclusion

This personal OS project has been an incredible journey of learning, dedication, and passion. Though it took over a year to complete, the experience has been invaluable. My hope is that others can use this project to better understand operating system concepts and gain hands-on experience in OS development. This project demonstrates that with the right motivation, commitment, and passion, it's possible to achieve great things even with limited time and resources.
Contact

If you have any questions, suggestions, or feedback, feel free to contact us. You can also open issues or submit pull requests to help improve this project!

Looking forward to seeing your contributions and having you join the development journey!

email:rockywang599@gmail.com
