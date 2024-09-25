# Use an official Ubuntu image as a base
FROM ubuntu:latest

# Update and install build essentials and necessary libraries
RUN apt-get update && apt-get install -y \
    build-essential \
    gcc

# Create a working directory
WORKDIR /usr/src/app

# Copy the source code into the container
COPY . .

# Compile the C application
RUN make all

# Expose the TCP port (e.g., port 8080)
EXPOSE 50002

# Run the application
CMD ["./pad_server/pad", "-f", "coldflow-fill.csv"]