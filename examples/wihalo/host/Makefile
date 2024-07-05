TARGET = cam_client
SRCS = cam_client.c multiwebcam_server.c

TARGET2 = dummy_server
SRCS2 = dummy_server.c

all: $(TARGET) $(TARGET2)

$(TARGET): $(SRCS)
	gcc -o $(TARGET) $(SRCS)

$(TARGET2): $(SRCS2)
	gcc -o $(TARGET2) $(SRCS2)

clean:
	rm -f $(TARGET) $(TARGET2)
