import torch
import torch.nn.functional as F
from torch.utils.data import Dataset, DataLoader, random_split
import pandas as pd
import numpy as np


# get dataframe
# divide first 6 rows by ~32000
# get sliding windows and label them based on the last 5-10? frames

class MotionDataSet(Dataset):
    def __init__(self, df, window_size=50, stride=1):
        raw_values = df.iloc[:, 0:6].values
        normalized_values = raw_values / 32768.0
        raw_labels = df.iloc[:, 6].values
        
        # we need to split up the data into 50 frame windows, and classify based on
        # raw labels
        self.windows = []
        self.window_labels = []
        
        for i in range(0, len(normalized_values) - window_size + 1, stride):
            window = normalized_values[i : i + window_size]
            # look at the fifth to last frame
            window_label = raw_labels[i + window_size - 6]
            
            self.windows.append(window)
            self.window_labels.append(window_label)

        self.windows = np.array(self.windows, dtype=np.float32)
        self.labels = np.array(self.window_labels, dtype=np.int64)
    
    def __len__(self):
        return len(self.labels)
    
    def __getitem__(self, index):
        return (
            torch.from_numpy(self.windows[index]), 
            torch.tensor(self.labels[index], dtype=torch.long)
        )
        
        
data = pd.read_csv('data.csv', header=None)
dataset = MotionDataSet(data)

g = torch.Generator().manual_seed(2147483647)

# split dataset into training and validation split
train_size = int(0.8 * len(dataset))
val_size = len(dataset) - train_size
train_dataset, val_dataset = random_split(dataset, [train_size, val_size], generator=g)
train_loader = DataLoader(train_dataset, batch_size=32, shuffle=True, generator=g)
val_loader = DataLoader(val_dataset, batch_size=32, shuffle=False)

# parameters of MLP
input_size = 300
output_size = 2
hidden_size = 100
learning_rate = 0.1
second_learning_rate = 0.05
epochs = 60

# making the MLP
W1 = torch.randn((input_size, hidden_size), generator=g) * 0.01
b1 = torch.zeros(hidden_size)
W2 = torch.randn((hidden_size, output_size), generator=g) * 0.01
b2 = torch.zeros(output_size)
parameters = [W1, b1, W2, b2]
for p in parameters:
    p.requires_grad_(True)

# training
for epoch in range(epochs):
    running_loss = 0.0
    total_samples = 0
    
    for batch_windows, batch_labels in train_loader:
        
        # flatten to vector of size 300 (window size * 6)
        Xb = batch_windows.view(batch_windows.size(0), -1)
        Yb = batch_labels

        # forward pass
        h = torch.tanh(Xb @ W1 + b1)
        logits = h @ W2 + b2
        loss = F.cross_entropy(logits, Yb)
        # print(loss.item())
        
        # backwards pass
        for p in parameters:
            p.grad = None
        loss.backward()
        
        # update
        for p in parameters:
            p.data += (-learning_rate if epoch < epochs / 2 else -second_learning_rate) * p.grad
        # track loss in epochs
        running_loss += loss.item() * len(Yb)
        total_samples += len(Yb)
    
    # val dataset check
    val_loss = 0.0
    with torch.no_grad():
        for batch_windows, batch_labels in val_loader:
            Xb = batch_windows.view(batch_windows.size(0), -1)
            Yb = batch_labels
            h = torch.tanh(Xb @ W1 + b1)
            logits = h @ W2 + b2
            loss = F.cross_entropy(logits, Yb)
            val_loss += loss.item() * len(Yb)
    
    # print loss
    epoch_loss = running_loss / total_samples
    epoch_val_loss = val_loss / len(val_dataset)
    print("training loss:", epoch_loss)
    print("validation loss", epoch_val_loss)

# simulating error rate
correct = 0
# track false positives and negatives
false_positives = 0
false_negatives = 0

with torch.no_grad():
    for batch_windows, batch_labels in val_loader:
        Xb = batch_windows.view(batch_windows.size(0), -1)
        Yb = batch_labels
        
        h = torch.tanh(Xb @ W1 + b1)
        logits = h @ W2 + b2
        
        preds = torch.argmax(logits, dim=1)
        correct += (preds == Yb).sum().item()
        false_positives += ((preds == 1) & (Yb == 0)).sum().item()
        false_negatives += ((preds == 0) & (Yb == 1)).sum().item()

total_val = len(val_dataset)
val_accuracy = (correct / total_val) * 100
val_error = 100.0 - val_accuracy

# print report
print("Total validation samples:", total_val)
print("Percentage correct:", val_accuracy)
print("Error Rate:", val_error)
print("Correct Predictions:", correct)
print("False Positives:", false_positives)
print("False Negatives:", false_negatives)

# write parameter to a c++ .h file

w1_array = W1.detach().cpu().numpy().flatten()
b1_array = b1.detach().cpu().numpy().flatten()
w2_array = W2.detach().cpu().numpy().flatten()
b2_array = b2.detach().cpu().numpy().flatten()

with open("src/params.h", "w") as f:
    f.write("#pragma once\n\n")
    
    f.write(f"#define INPUT_SIZE {input_size}\n")
    f.write(f"#define HIDDEN_SIZE {hidden_size}\n")
    f.write(f"#define OUTPUT_SIZE {output_size}\n\n")
    
    def write_array(name, arr):
        f.write(f"const float {name}[{len(arr)}] = {{\n ")
        vals_formatted = [f"{v:.8f}f" for v in arr]
        f.write(", ".join(vals_formatted))
        f.write("\n};\n\n")
    write_array("W1", w1_array)
    write_array("b1", b1_array)
    write_array("W2", w2_array)
    write_array("b2", b2_array)