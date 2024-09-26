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

# Compile application
RUN make all

# Expose TCP port for control socket
EXPOSE 50001/TCP

# Expose TCP port telemetry socket
EXPOSE 50002/TCP

# Run the application using coldflow-fill.csv data
CMD ["./pad_server/pad", "-f", "coldflow-fill.csv"]