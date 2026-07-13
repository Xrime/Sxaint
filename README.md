# Sxaint

Sxaint is a fast and secure file transfer application, currently supported only on Windows PCs. It is designed for local networks and the global internet. It is built with a heavy emphasis on zero-copy memory efficiency, multi-threading, network reliability, and a nice dynamic user interface.


## **_Application Architecture and Technology Stack_**

### User Interface Layer and Frontend Integration

• The application is built on a modular C++ backend bridging to a highly responsive frontend powered by Slint.
• The UI is decoupled from the heavy C++ logic and written in declarative Slint (appwindow.slint).
• It features: Modern Aesthetics: Deep dark themes, radiant gradients, rounded custom components, and smooth hover/click micro-animations.
• Dynamic Navigation: A custom-built hamburger menu with a fluid slide-out drawer routing between Home, Transfer, and History views.

### Network and Transport Layer Implementation

• Network & Transport Layer (src/net)
• Sxaint circumvents the slow overhead of traditional TCP using a custom UDP stack backed by KCP to guarantee reliability while achieving maximum throughput.

### Enhanced Data Transmission with KCP

• KCP Transport (transport.cpp): Wraps raw UDP sockets with the KCP protocol.
• This provides ARQ (Automatic Repeat Request), congestion control, and stream assembly, resulting in 10-40% faster transfers than TCP on lossy networks.

### Effortless Local Network Device Detection

• Local Discovery (discovery.cpp): Uses UDP broadcasts on port 9001 to instantly locate other Sxaint instances on the local network (e.g., "xint2's PC") without requiring manual IP entry.

### Internet-Wide Transfer Capability via STUN (still in development)

• GI Mode / STUN Client (stun.cpp): "Global Internet" mode allows transfers across the internet.
• It dynamically fetches the user's public IP using a lightweight STUN client querying Google's STUN servers (stun.l.google.com), parsing XOR-MAPPED-ADDRESS attributes to resolve NAT traversal.

### Centralized Transfer Lifecycle Control

• Session Management (session.cpp): The brain of the transfer.
• Manages the lifecycle of a transfer, handling the Handshake protocol, dispatching threads to process chunks, and managing resume functionality.

### Memory Mapping for Efficient File Operations

• The Core layer handles extreme optimization for disk I/O, security, and CPU utilization.
• Memory Mapping (file_mapper.cpp): Replaces slow standard fstream operations with OS-level Memory Mapped Files (MapViewOfFile).
• For receivers, it uses Sparse Files (FSCTL_SET_SPARSE) and SetEndOfFile to instantly preallocate massive files (even gigabytes in size) in milliseconds without thrashing the disk with zeroes.
• It employs auto-renaming to gracefully avoid ERROR_SHARING_VIOLATION.

### Optimizing Performance with Concurrency and Compression

• Concurrency (thread_pool.cpp): A custom Thread Pool asynchronously handles the heavy lifting of reading, compressing, and encrypting data chunks simultaneously, maximizing multi-core CPU usage.
• Chunker (chunker.cpp): Slices massive mapped files into manageable payloads.
• Smart Compressor (compressor.cpp): Inspects the file entropy on the fly and applies intelligent compression to chunks, reducing the bandwidth required to send text/data files.

### Secure Data Encryption

• Cryptography (crypto.cpp): Derives a strong AES encryption key directly from the 6-digit PIN code.
• Every chunk payload is encrypted over the wire, ensuring completely secure, end-to-end encrypted transfers.

### Resumable Transfers with Manifests

• Manifest & Resume (manifest.cpp): Generates a bitfield of received chunks.
• If a transfer drops, the Receiver sends the bitfield during the next handshake, allowing the Sender to instantly skip existing chunks and resume exactly where it left off.

### Ensuring Data Integrity

• Integrity (hasher.cpp): Calculates extremely fast CRC32 checksums for every individual chunk to guarantee that zero data corruption occurs during transmission.

## **How It Works (The Transfer Lifecycle)**

### Preparation and Discovery

• The Sender selects a file.
• The FileMapper opens the file for GENERIC_READ and maps it instantly into RAM.
• If GI Mode is checked, the Receiver uses the stunClient to find its public IP and gives it to the Sender.
• Otherwise, the Sender uses the Local Discovery service to find the Receiver.

### Establishing the Connection and Acknowledgment

• The Sender packages the file metadata (Name, Size, Total Chunks, and a hashed version of the 6-digit PIN) into a handshake struct and shoots it over the UDP socket.
• The Receiver captures the handshake.
• If the PIN matches, it uses file_mapper to instantly preallocate a Sparse File on its local drive.
• It then responds with a handshakeAck containing the Resume Bitfield.

### Data Processing and Transfer Initiation

• The Sender's Session slices the memory-mapped file into arrays of bytes.
• The ThreadPool takes over.

### Transmission and Data Integrity Checks

• Threads concurrently grab chunks, apply smartCompressor, encrypt them via Crypto (using the AES key derived from the PIN), append a chunkWireHeader (containing the chunk ID and CRC32 hash), and push them to KCPTransport.
• The Receiver's KCP stack catches the UDP packets, ensuring they arrive in order without drops.
• The Receiver's ThreadPool decrypts the payload, decompresses it, verifies the CRC32 hash, and writes it directly into the memory-mapped target file at the exact offset (chunk_id * chunk_size).

### Completion and Logging

• The Session tracks progress via metrics.cpp, pushing real-time Speed (MB/s) and ETA updates to the Slint UI.
• Once all chunks are written, the file handles are safely closed, and the transfer is permanently logged in the UI's History view via the C++ log_history lambda.