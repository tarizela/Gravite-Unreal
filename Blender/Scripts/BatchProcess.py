
import os
import subprocess
import threading
import time

SOURCE_DIR = 'F:\\GravityRushVR\\Gravity-Rush-VR-Experimental\\Assets\\Models\\'
OUTPUT_DIR = 'C:\\Users\\caine\\Documents\\GitHub\\Gravity\\FBX2\\'
BLENDER_EXEC = 'C:\\Program Files\\Blender Foundation\\Blender 3.3\\blender.exe'
SCRIPT = 'GR2_to_UE.py'
MAX_WORKERS = 32  # Number of threads

console_lock = threading.Lock()

# def clear_line():
#     print("\033[K", end='')

# def set_cursor_line(line_num):
#     print(f"\033[{line_num}H", end='')

line_numbers = {}
current_line = 1

def process_fbx(fbx_file, index, total):
    global current_line

    # Find first false in active_task
    task_id = active_task.index(False)
    active_task[task_id] = True

    with console_lock:
        print(f"[{index}/{total}][Task #{task_id}]: {fbx_file}: Processing...")
        # clear_line()
        # print()  # Move to the next line
        # line_numbers[fbx_file] = current_line  # Assign a line number to the FBX file
        # current_line += 1  # Increment the current line

    fileName = os.path.splitext(fbx_file)[0]
    fileOutputFolder = os.path.join(OUTPUT_DIR, fileName)
    output_file = os.path.join(fileOutputFolder, fbx_file)
    
    if not os.path.exists(fileOutputFolder):
        os.makedirs(fileOutputFolder)
    
    if os.path.exists(output_file):
        with console_lock:
            # set_cursor_line(current_line)
            print(f"[{index}/{total}][Task #{task_id}]: {fbx_file}: Already exist, Skipped") #, end='')
            # clear_line()
            # print()  # Move to the next line
        active_task[task_id] = False
        semaphore.release()  # Release the semaphore when the thread is done
        return

    start_time = time.time()

    command = [
        BLENDER_EXEC, '-b', '-P', SCRIPT, '--',
        '-SOURCE', os.path.join(SOURCE_DIR, fbx_file),
        '-OUTPUT', output_file
    ]

    try:
        # subprocess.run(command, creationflags=subprocess.CREATE_NEW_CONSOLE, check=True)
        subprocess.run(command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)

        elapsed_time = time.time() - start_time
        with console_lock:
            # set_cursor_line(line_numbers[fbx_file])
            print(f"[{index}/{total}][Task #{task_id}]: {fbx_file}: Done. Time used: {elapsed_time:.2f}s") #, end='')
            # clear_line()
            # print()  # Move to the next line
    except subprocess.CalledProcessError:
        with console_lock:
            # set_cursor_line(line_numbers[fbx_file])
            print(f"[{index}/{total}][Task #{task_id}]: {fbx_file}: Error")  #, end='')
            # clear_line()
            # print()  # Move to the next line
    finally:
        active_task[task_id] = False
        semaphore.release()  # Release the semaphore when the thread is done

fbx_files = [f for f in os.listdir(SOURCE_DIR) if f.lower().endswith('.fbx')]

# Create a thread pool and limit the number of workers
semaphore = threading.Semaphore(MAX_WORKERS)
active_task = [False] * MAX_WORKERS

threads = []

try:
    for idx, fbx in enumerate(fbx_files, 1):
        semaphore.acquire()
        t = threading.Thread(target=process_fbx, args=(fbx, idx, len(fbx_files)))
        t.start()
        threads.append(t)

    # Wait for all threads to complete
    for t in threads:
        t.join()
except KeyboardInterrupt:
    print("KeyboardInterrupt")
    exit(1)