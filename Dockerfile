FROM ghcr.io/wiiu-env/devkitppc:20240505

WORKDIR /project

# Copy project files
COPY . .

# Create directory structure if it doesn't exist
RUN mkdir -p source build

# Move main.c to source directory if it's in root
RUN if [ -f main.c ]; then mv main.c source/; fi

# Build the project
CMD ["make"]