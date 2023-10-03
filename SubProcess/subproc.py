import subprocess

# Replace with the actual path to your executable
exe_path = "enrol.exe"

# Use the start command to run the executable
command = f"start /B /wait {exe_path}"
process = subprocess.Popen(
    command,
    shell=True,
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,
    text=True,
)

output = process.stdout.readline().strip()
print(output)

process.stdin.write("0\n")
process.stdin.flush()

output = process.stdout.readline().strip()
print(output)

process.wait()
