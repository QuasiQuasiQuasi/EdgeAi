import serial
import sqlite3
from config import *

def main() -> None:
	with sqlite3.connect(DATABASE_PATH) as con:
		for device in DEVICES.values():
			device.create_table(con, JOINED_CODEC)
		with serial.Serial(RECEIVER_PORT, BAUD_RATE) as ser:
			ser.read_all()
			for i in range(8000):
			# while True:
				data = ser.read_until(expected=b"end\n")
				if not data[0] == ord("@"): continue
				id = next(HEADER_CODEC.parse(data[1::]))
				if not id in DEVICES:
					print(f"New device with id: {hex(id)} has connected!")
					continue
				DEVICES[id].insert_into_table(con, JOINED_CODEC, data[HEADER_CODEC.position + 1::])



if __name__ == "__main__":
	main()
