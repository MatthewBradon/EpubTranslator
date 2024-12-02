from transformers import AutoTokenizer
import sys

# Initialize tokenizer once (can be shared across workers)
tokenizer = AutoTokenizer.from_pretrained('Helsinki-NLP/opus-mt-ja-en')

# Tokenize text and return tensors
def tokenize_text(text):
    inputs = tokenizer(text, return_tensors="pt")
    # Check encoding of text


    print(f"\n\n\n\nOriginal Sentence: {text}")
    print(f"Tokenized Input: {inputs}")
    sys.stdout.flush()
    return inputs

# Detokenize text (accepts tokenized arrays)
def detokenize_text(text):
    print(f"Decoder output tokens: {text}")
    sys.stdout.flush()
    decodetext = tokenizer.decode(text, skip_special_tokens=True)
    return decodetext

def func_print(text):
    print(text)