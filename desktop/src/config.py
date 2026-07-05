# --- CONFIGURATION ---
DATA_FOLDER = "../MHOOP_dataset"
OUTPUT_FOLDER = "../MHOOP_outputs"
WINDOW_SIZE = 150         # 3 seconds @ 50Hz
STEP_SIZE = 75            # 50% overlap
EPOCHS = 200
BATCH_SIZE = 32


TABLE_NAME = "accelerometer_data"
ORDER_TABLE_BY = "local_timestamp" 
SENSOR_DATA_COLUMNS = ["x", "y", "z"]
DATA_DIMENSIONS = len(SENSOR_DATA_COLUMNS)
LABLE_COLUMN = "lable"

KERNEL_SIZE = 3


COLORS = ["r", "g", "b"]