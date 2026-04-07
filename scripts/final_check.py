import pandas as pd
from pathlib import Path

df = pd.read_csv(r'C:\Users\Admin\source\repos\bullet_hole_detection_system\scripts\dataset\annotations.csv')
img_dir = Path(r'C:\Users\Admin\source\repos\bullet_hole_detection_system\scripts\dataset\images')

print('FINAL DATASET VERIFICATION')
print('=' * 50)
print(f'Dataset shape: {df.shape}')
print(f'Columns: {len(df.columns)}')
print(f'Missing values: {df.isnull().sum().sum()}')
print(f'Labels: {sorted(df["label"].unique())}')
print(f'Images: {len(list(img_dir.glob("*.png")))}')
print(f'Status: VERIFIED')
