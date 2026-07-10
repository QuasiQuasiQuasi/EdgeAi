from codec import *

# Database
DATABASE_PATH = "../../MHOOP_dataset/joystick.db"
TABLE_NAME = "accelerometer_data"

ORDER_TABLE_BY = LOCAL_TIMESTAMP 
LABLE_COLUMN = "lable"


# Training
OUTPUT_FOLDER = "../../MHOOP_outputs"
WINDOW_SIZE = 150         # 3 seconds @ 50Hz
STEP_SIZE = 75            # 50% overlap
EPOCHS = 200
BATCH_SIZE = 32



KERNEL_SIZE = 3


COLORS = ["r", "g", "b"]



DEVICES: dict[int, Device] = {
	0xe539: Device("idle")
}


# Codec
HEADER_CODEC = Codec([CodecValue("id", CodecValueType.INT, 2)])
TIME_CODEC   = Codec([CodecValue("device_timestamp", CodecValueType.INT, 4)])
DATA_CODEC   = Codec([CodecValue("x", CodecValueType.INT, 2), CodecValue("y", CodecValueType.INT, 2)])

JOINED_CODEC = Codec(TIME_CODEC + DATA_CODEC)

# Reciever
RECEIVER_PROTOCOLL = ReceiverProtocoll.UART
RECEIVER_PORT = "/dev/ttyACM2"
BAUD_RATE = 115200