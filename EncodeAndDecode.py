from transformers import AutoTokenizer
from optimum.onnxruntime import ORTModelForSeq2SeqLM
import multiprocessing as mp
import sys
import re
import torch

# Global variable to store the model in each worker
model = None
onnx_model_path = 'onnx-model-dir'

# Initialize tokenizer once (can be shared across workers)
tokenizer = AutoTokenizer.from_pretrained('Helsinki-NLP/opus-mt-ja-en')

# Function to initialize the model once per worker
def init_model():
    global model
    if model is None:
        print("Loading model in worker process.")
        sys.stdout.flush()
        model = ORTModelForSeq2SeqLM.from_pretrained(onnx_model_path)

def process_task(task):
    """Process a single task in a worker."""
    encoded_data, chapterNum, position = task

    print(f"Processing chapter {chapterNum} at position {position}.")
    sys.stdout.flush()

    init_model()  # Initialize model in worker

    try:
        print("Starting model inference.")
        sys.stdout.flush()
        generated = model.generate(**encoded_data)
        print(f"Generated: {generated[0]}")
        sys.stdout.flush()
        
        # Detokenize
        text = tokenizer.decode(generated[0], skip_special_tokens=True)
        print(text)  
        return chapterNum, position, text
    except Exception as e:
        print(f"Error while processing chapter {chapterNum} at position {position}: {str(e)}")
        sys.stdout.flush()
        return None

def parse_tensor_string(tensor_str):
    # Replace 'tensor([...])' with just the list inside '[...]'
    return re.sub(r"tensor\((\[.*?\])\)", r"\1", tensor_str)

def readEncodedData(file_path):
    """Read and parse encoded data from a file."""
    tag_dict = {}
    counter = 0
    with open(file_path, 'r') as file:
        for line in file:
            # Split the line into chapter, position, and tensor data
            parts = line.strip().split(',', 2)
            if len(parts) != 3:
                continue  # Skip lines that don't match the expected format

            chapter = int(parts[0])
            position = int(parts[1])

            # Pre-process the tensor data to remove 'tensor(...)'
            processed_tensor_data = parse_tensor_string(parts[2])

            # Convert the processed string into a Python dictionary
            tensor_data = eval(processed_tensor_data)  # Use eval after sanitizing

            tensor_data = {
                "input_ids": torch.tensor(tensor_data["input_ids"], dtype=torch.long),
                "attention_mask": torch.tensor(tensor_data["attention_mask"], dtype=torch.long),
            }


            # Use (chapter, position) as the key
            tag_dict[(chapter, position)] = tensor_data
            counter += 1

    return tag_dict

def run_model_multiprocessing(file_path, num_workers=2):
    """Function to distribute tasks among workers using multiprocessing.Pool."""
    # Read encoded data from the file
    inputs = readEncodedData(file_path)

    # Prepare tasks for workers
    tasks = [(encoded_data, chapterNum, position) for (chapterNum, position), encoded_data in inputs.items()]
    mock_tasks = tasks[:50]


    # Create a pool of worker processes
    with mp.Pool(processes=num_workers) as pool:
        # Submit tasks to the pool
        # results = pool.map(process_task, tasks)
        results = pool.map(process_task, mock_tasks)

    # Filter out None results (if any errors occurred)
    results = [result for result in results if result is not None]

    if not results:
        print("No results were processed by the workers.")
        sys.stdout.flush()
    else:
        print(f"Processed {len(results)} results.")
        sys.stdout.flush()

    print("Main process exiting.")
    sys.stdout.flush()

    return results

def main(file_path):
    """Parent process that spawns workers."""
    print("Starting parent process.")
    sys.stdout.flush()

    # Set the spawn start method
    mp.set_start_method("spawn", force=True)

    num_workers = 4

    # Run the multiprocessing task
    results = run_model_multiprocessing(file_path, num_workers)

    # Write out results to translatedTags.txt
    with open("translatedTags.txt", "w", encoding="utf-8") as file:
        for result in results:
            file.write(f"{result[0]},{result[1]},{result[2]}\n")
    
    return 0
            
        
if __name__ == "__main__":
    file_path = "encodedTags.txt"
    main(file_path)
