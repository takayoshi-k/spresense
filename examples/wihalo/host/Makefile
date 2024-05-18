TARGET = cam_client
SRCS = cam_client.c multiwebcam_server.c

all: $(TARGET)

$(TARGET): $(SRCS)
	gcc -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)
