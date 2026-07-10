import sqlite3
import time
import threading
from enum import Enum
from dataclasses import dataclass
from typing import Generator

LOCAL_TIMESTAMP = "local_timestamp"

class CodecValueType(Enum):
	INT = 0

	def get_sql_type_name(self) -> str:
		match self:
			case CodecValueType.INT:
				return "INTEGER"
		return ""

class ReceiverProtocoll(Enum):
	UART = 0
	USB = 1



@dataclass
class CodecValue:
	name: str
	type: CodecValueType
	size_bytes: int
	signed: bool = False
	primary: bool = False


class Codec(list[CodecValue]):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)
		self.position: int = 0


	def parse(self, buffer: bytes) -> Generator[any, None, None]:
		self.position = 0
		for value in self:
			match value.type:
				case CodecValueType.INT:
					if value.size_bytes > len(buffer) - self.position: return
					self.position += value.size_bytes
					yield int.from_bytes(buffer[self.position - value.size_bytes:self.position], "little", signed = value.signed)
	
	def get_columns(self) -> list[str]:
		return list(value.name for value in self)

	def get_dimensions(self) -> int:
		return len(self)







class Device:
	def __init__(self, name: str = ""):
		self.name = name
		# self.df: pd.DataFrame = None

	def create_table(self, con: sqlite3.Connection, codec: Codec) -> None:
		if not self.name: return
		
		con.execute(f"CREATE TABLE IF NOT EXISTS {self.name}({LOCAL_TIMESTAMP} INTEGER PRIMERY KEY, {", ".join(f"{value.name} {value.type.get_sql_type_name()}" for value in codec)});")
	
	def get_local_timestamp(self) -> int:
		return int(time.time_ns() // 1_000_000)

	def insert_into_table(self, con: sqlite3.Connection, codec: Codec, buffer: bytes) -> None:
		if not self.name: return

		print(f"INSERT INTO {self.name} VALUES({self.get_local_timestamp()}, {", ".join(str(x) for x in codec.parse(buffer))});")
		con.execute(f"INSERT INTO {self.name} VALUES({self.get_local_timestamp()}, {", ".join(str(x) for x in codec.parse(buffer))});")


