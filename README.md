# ext2 File System Shell (Linux-Commands)

<p align="center">
  <img src="https://img.shields.io/badge/Language-C-blue.svg" alt="Language C">
  <img src="https://img.shields.io/badge/Platform-Linux-lightgrey.svg" alt="Platform Linux">
  <img src="https://img.shields.io/badge/Status-Completed-green.svg" alt="Status Completed">
</p>

<p align="center">
  An interactive shell built from scratch in C to navigate and inspect an `ext2` file system image, demonstrating a deep understanding of low-level file system architecture and direct disk access.
</p>

<p align="center">
  <img src="https://i.imgur.com/REPLACE_THIS_WITH_YOUR_GIF_URL.gif" alt="Project Demo GIF" width="800"/>
</p>

## About The Project

[cite_start]This project is an implementation of an interactive shell designed to manipulate and analyze `ext2` file system images, which are based on the Berkley Fast File System (FFS)[cite: 14]. [cite_start]The primary goal was to develop a toolset capable of navigating the internal structure of a disk image, allowing for operations like file listing, directory traversal, and metadata inspection by directly accessing disk blocks[cite: 15, 16].

[cite_start]The entire project was developed in **C**, using the data structures defined by the `ext2` system to interpret raw disk data without relying on the host operating system's file system drivers[cite: 16].

## Key Features

The shell provides a set of essential commands to explore the `ext2` image:

* [cite_start]**`ls`**: Lists all files and subdirectories in the current directory, reading directory entries from data blocks[cite: 89, 92].
* [cite_start]**`cd`**: Navigates through the directory hierarchy by resolving directory names to their corresponding inodes[cite: 120, 121].
* [cite_start]**`stat`**: Displays detailed metadata for any given file or directory, including permissions, size, timestamps, and data block pointers stored in its inode[cite: 132, 134, 135].
* [cite_start]**`find`**: Performs a recursive search from the current directory to map and display the entire underlying file hierarchy[cite: 151, 153].
* [cite_start]**`sb`**: Reads and displays global metadata from the filesystem's superblock, such as total blocks, inode counts, and block size[cite: 168, 169].

## Tech Stack

* **C Programming Language**
* **ext2 File System Principles**
* **Direct Disk I/O Operations**

## Getting Started

To get a local copy up and running, follow these simple steps.

1.  **Clone the repo**
    ```sh
    git clone [https://github.com/Gronoxx/Linux-Commands.git](https://github.com/Gronoxx/Linux-Commands.git)
    ```
2.  **Navigate to the project directory**
    ```sh
    cd Linux-Commands
    ```
3.  **Compile the project** (A Makefile is usually required for C projects)
    ```sh
    make
    ```
4.  **Run the shell** with an `ext2` disk image
    ```sh
    ./dcc-fsshell <path_to_your_filesystem_image.img>
    ```

## My Contribution

[cite_start]This project was developed in a team of three for the Operating Systems course[cite: 3, 7]. [cite_start]Due to unforeseen circumstances with another team member, our group was formed at the last minute to tackle this complex assignment[cite: 8, 9, 10].

[cite_start]In this challenging context, I collaborated equally on all fronts of the project, from design to implementation[cite: 11]. My key contributions included:

* [cite_start]Designing the in-memory data structures for the superblock, group descriptors, and inodes[cite: 19].
* [cite_start]Implementing the logic for the `stat` and recursive `find` commands, which required deep traversal of directory entries and inodes[cite: 133, 153].
* Developing the core functions for reading data directly from disk blocks at specific offsets.
* Debugging low-level memory management issues and disk read errors.

## Course Context

This was the final project for the **Operating Systems (DCC605)** course at **Universidade Federal de Minas Gerais (UFMG)**. [cite_start]The project was instrumental in providing a practical understanding of modern file system concepts like inodes, block allocation, and directory structures[cite: 183, 184].
