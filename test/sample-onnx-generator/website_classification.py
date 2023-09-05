import torch
from transformers import AutoTokenizer, AutoModelForSequenceClassification, AutoConfig


def hook_fn(module, input, output):
    return torch.argmax(output.logits, dim=-1)


def main():
    # Load model directly

    model_name = "alimazhar-110/website_classification"
    tokenizer = AutoTokenizer.from_pretrained(model_name)
    model = AutoModelForSequenceClassification.from_pretrained(model_name)
    model.register_forward_hook(hook_fn)
    model.eval()

    inputs = tokenizer([
        "ChatGPT Creates Arduino Drivers in the Style of Adafruit's Ladyada",
        "Global financial watchdog warns of ‘further challenges and shocks’ ahead",
        "What wine should I bring to a divorce party?"
    ],
        truncation=True,
        return_tensors='pt', padding='max_length', max_length=512)

    outputs = model(**inputs)

    print(inputs.input_ids.tolist())
    print(inputs.attention_mask.tolist())
    print(inputs, outputs)
    # print([model.labels[pred.item()] for pred in outputs])
    # exit(0)

    input_ids = torch.ones(1, 512, dtype=torch.int64)
    attention_mask = torch.ones(1, 512, dtype=torch.int64)

    torch.onnx.export(
        model,
        (input_ids, attention_mask),
        "../fixture/website_classification.onnx",
        input_names=['input_ids', 'onnx::Equal_1'],
        output_names=['output'],
        dynamic_axes={'input_ids': {0: 'batch_size'}, 'onnx::Equal_1': {0: 'batch_size'}, 'output': {0: 'batch_size'}},
    )


if __name__ == '__main__':
    main()
