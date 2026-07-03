import sqlite3
import numpy as np
import pandas as pd
import glob
import os

from config import *


def merge_databases(folder_path: str) -> None:

	db_files: list[str] = glob.glob(os.path.join(folder_path, "single/*.db"))
	
	if not db_files:
		print("No .db files found! Make sure your files are in the 'datasets' folder.")
		return
	
	print(f"Found {len(db_files)} database files.")
	
	with sqlite3.connect(os.path.join(folder_path, "dataset.db")) as out:


		for db_file in db_files:
			filename: str = os.path.basename(db_file)
			label: str = os.path.splitext(filename)[0]
			
			print(f"Loading '{filename}' as class: '{label}'...")
			
			try:
				with sqlite3.connect(db_file) as conn:
					df: pd.DataFrame = pd.read_sql_query(f"SELECT * FROM {TABLE_NAME}", conn)

					df.to_sql(label, out)

			except Exception as e:
				print(f"  -> Error loading {db_file}: {e}")

merge_databases(DATA_FOLDER)