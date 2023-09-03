import torch
from transformers import AutoTokenizer, AutoModelForSequenceClassification, AutoConfig


def main():
  # Load model directly

  config = AutoConfig.from_pretrained("alimazhar-110/website_classification")
  tokenizer = AutoTokenizer.from_pretrained("alimazhar-110/website_classification")
  model = AutoModelForSequenceClassification.from_pretrained("alimazhar-110/website_classification")

  inputs = tokenizer(["ChatGPT Creates Arduino Drivers in the Style of Adafruit's Ladyada"], truncation=True,
                     return_tensors='pt', padding='max_length', max_length=512)

  with torch.no_grad():
    outputs = model(**inputs)

  predicted_class_idx = torch.argmax(outputs.logits, dim=1).item()

  print(inputs, outputs)
  print(predicted_class_idx, config.id2label[predicted_class_idx])

  torch.onnx.export(
    model,
    (inputs.input_ids, inputs.attention_mask),
    "../fixture/website_classification.onnx",
    export_params=True,
    input_names=['input_ids', 'attention_mask'],
    dynamic_axes={'input_ids': {0: 'batch_size'}, 'attention_mask': {0: 'batch_size'}},
    output_names=['output'],
    verbose=True,
  )


if __name__ == '__main__':
  main()
