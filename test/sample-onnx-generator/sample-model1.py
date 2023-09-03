import numpy as np
import torch
from torch import nn


class SimpleModel(nn.Module):
    def __init__(self):
        super().__init__()

        self.linear1 = torch.nn.Linear(3, 3)
        self.linear2 = torch.nn.Linear(3, 1)

    def forward(self, x, y, z):
        return self.linear2(self.linear1(torch.cat((x, y, z), dim=1)))


def main():
    x = np.random.randint(10, size=(10000, 1))
    y = np.random.randint(10, size=(10000, 1))
    z = np.random.randint(10, size=(10000, 1))
    labels = np.random.randint(3, size=(10000, 1))

    model = SimpleModel()

    epochs = 20
    batch_size = 100
    lr = 0.0001

    loss_fn = torch.nn.MSELoss(reduction='mean')
    optimizer = torch.optim.Adam(model.parameters(), lr=lr)

    max_accuracy = 0.0
    state_dict = None
    for epoch in range(epochs):
        count = 0
        match = 0

        for i in range(0, len(x), batch_size):
            batch_x = torch.tensor(x[i:i + batch_size], dtype=torch.float32)
            batch_y = torch.tensor(y[i:i + batch_size], dtype=torch.float32)
            batch_z = torch.tensor(z[i:i + batch_size], dtype=torch.float32)
            batch_labels = torch.tensor(labels[i:i + batch_size], dtype=torch.float32)

            model.zero_grad()
            predict = model(batch_x, batch_y, batch_z)
            loss = loss_fn(predict, batch_labels)
            loss.backward()
            optimizer.step()

            count += len(batch_labels)
            match += (torch.round(predict) == batch_labels).sum().item()

        accuracy = (torch.round(predict) == batch_labels).sum().item() / len(batch_labels)
        if accuracy > max_accuracy:
            max_accuracy = accuracy
            state_dict = model.state_dict()

        print(f"epoch: {epoch}, loss: {loss.item()}, accuracy: {accuracy}")

    print(f"max_accuracy: {max_accuracy}")
    print(f"state_dict: {state_dict}")

    model_to_save = SimpleModel()
    model_to_save.load_state_dict(state_dict)

    torch.onnx.export(
        model_to_save,
        (torch.tensor(x[:1], dtype=torch.float32), torch.tensor(y[:1], dtype=torch.float32),
         torch.tensor(z[:1], dtype=torch.float32)),
        "../fixture/sample-model1.onnx",
        export_params=True,
        input_names=['x', 'y', 'z'],
        dynamic_axes={'x': {0: 'batch_size'}, 'y': {0: 'batch_size'}, 'z': {0: 'batch_size'}, 'output': {0: 'batch_size'}},
        output_names=['output'],
        verbose=True,
    )


if __name__ == '__main__':
    main()
