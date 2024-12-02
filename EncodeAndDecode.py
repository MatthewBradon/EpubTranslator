from transformers import AutoTokenizer
from optimum.onnxruntime import ORTModelForSeq2SeqLM
import multiprocessing as mp
import sys

# Global variable to store the model in each worker
model = None
onnx_model_path = 'onnx-model-dir'

# Set start method for multiprocessing
mp.set_start_method("fork", force=True)

class encodedDataSingleton:
    _instance = None
    _encodedDataDict = None

    @staticmethod
    def get_instance():
        if encodedDataSingleton._instance is None:
            encodedDataSingleton()
        return encodedDataSingleton._instance

    def __init__(self):
        if encodedDataSingleton._instance is not None:
            raise Exception("This class is a singleton!")
        else:
            encodedDataSingleton._instance = self

    def set_encodedDataDict(self, data):
        print("Setting encoded data dict")
        sys.stdout.flush()
        self._encodedDataDict = data

    def get_encodedDataDict(self):
        return self._encodedDataDict
    
    def print_singletonDict(self):
        for values in self._encodedDataDict.values():
            print(values)
            sys.stdout.flush()

    def print_keys(self):
        print(self._encodedDataDict.keys())
        sys.stdout.flush()

# Initialize tokenizer once (can be shared across workers)
tokenizer = AutoTokenizer.from_pretrained('Helsinki-NLP/opus-mt-ja-en')

# Function to initialize the model once per worker
def init_model():
    global model
    if model is None:
        print("Loading model in worker process.")
        sys.stdout.flush()
        model = ORTModelForSeq2SeqLM.from_pretrained(onnx_model_path)

def process_task(encoded_data):
    """Process a single task in a worker."""
    encoded, chapterNum, position = encoded_data

    print(f"Processing chapter {chapterNum} at position {position}.")
    sys.stdout.flush()

    init_model()  # Initialize model in worker

    try:
        print(f"Starting model inference.")
        sys.stdout.flush()
        generated = model.generate(**encoded)
        print(f"Generated: {generated[0]}")
        sys.stdout.flush()
        return generated[0], chapterNum, position
    except Exception as e:
        print(f"Error while processing chapter {chapterNum} at position {position}: {str(e)}")
        sys.stdout.flush()
        return None

def run_model_multiprocessing(inputs, num_workers=2):
    """Function to distribute tasks among workers using multiprocessing.Pool."""
    # Create a pool of worker processes
    with mp.Pool(processes=num_workers) as pool:
        tasks = [(encoded_data, chapterNum, position) for (chapterNum, position), encoded_data in inputs.items()]

        # Submit tasks to the pool
        results = pool.map(process_task, tasks)

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
