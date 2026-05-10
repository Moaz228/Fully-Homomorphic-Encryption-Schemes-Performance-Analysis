import os
import sys

import pandas as pd


def analyze():
    # 1. Get CSV filename from command line argument or use default
    if len(sys.argv) > 1:
        csv_name = sys.argv[1]
    else:
        csv_name = input("Enter the CSV filename (e.g., metrics.csv): ").strip()

    if not os.path.exists(csv_name):
        print(f"❌ Error: File '{csv_name}' not found.")
        return

    try:
        # 2. Load the data
        df = pd.read_csv(csv_name)

        # 3. Clean the data: Remove rows that have ANY empty/NaN values
        # This handles the "some rows won't record" issue
        initial_count = len(df)
        df_clean = df.dropna()
        final_count = len(df_clean)

        if final_count == 0:
            print("⚠️ No complete records found (all rows had missing data).")
            return

        # 4. Identify Schema and Operation (taking the most frequent if mixed)
        schema = df_clean["Schema"].iloc[0]
        operation = df_clean["Operation"].iloc[0]

        # 5. Filter for Time Metrics only (ms)
        # We look for columns that contain 'ms' or 'cpu' and exclude 'size' or 'kb'
        time_columns = [
            col for col in df_clean.columns if "_ms" in col or "_cpu" in col
        ]

        # 6. Calculate Averages
        averages = df_clean[time_columns].mean()

        # --- OUTPUT ---
        print("\n" + "=" * 50)
        print(f"ANALYSIS FOR: {schema} - {operation}")
        print(
            f"RECORDS USED: {final_count} (Dropped {initial_count - final_count} incomplete rows)"
        )
        print("=" * 50)
        print(f"{'METRIC (Time/CPU)':<25} | {'AVERAGE':<15}")
        print("-" * 50)

        for column, value in averages.items():
            unit = "ms" if "_ms" in column else "% CPU"
            print(f"{column:<25} | {value:>10.4f} {unit}")

        print("=" * 50 + "\n")

        # Optional: Save the cleaned data back to a new file
        # df_clean.to_csv("cleaned_" + csv_name, index=False)

    except Exception as e:
        print(f"❌ An error occurred: {e}")


if __name__ == "__main__":
    analyze()
