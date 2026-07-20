import torch
import torch.nn.functional as F
from torch.utils.data import Dataset, DataLoader
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
        self.labels = np.array(self.window_labels, dtype=np.long)
    
    def __len__(self):
        return len(self.labels)
    
    def __getitem__(self, index):
        return (
            torch.tensor(self.windows[index], dtype=torch.float32), 
            torch.tensor(self.labels[index], dtype=torch.int64)
        )
        
        
data = pd.read_csv('data.csv', header=None)
dataset = MotionDataSet(data)
print(dataset.__getitem__(10))

data_loader = DataLoader(dataset, batch_size=32, shuffle=True)











# input_size = 4
# output_size = 2
# hidden_size = 100

# W1 = torch.randn(input_size, hidden_size)
# b1 = torch.randn(hidden_size)
# W2 = torch.randn(hidden_size, output_size)
# b2 = torch.randn(output_size)
# parameters = [W1, b1, W2, b2]
# for p in parameters:
#     p.requires_grad_(True)

# Xs = torch.tensor(
#     [[1, 1, 1, 1],
#     [0, 0, 0, 0],
#     [1, 2, 3, 4],
#     [5, 5, 5, 5]]
# ).float()
# Ys = torch.tensor([0, 1, 1, 0])

# for _ in range(100):
    
#     # forward pass
#     h = torch.tanh(Xs @ W1 + b1)
#     logits = h @ W2 + b2
#     loss = F.cross_entropy(logits, Ys)
#     print(loss.item())
    
#     # backwards pass
#     for p in parameters:
#         p.grad = None
#     loss.backward()
    
#     # update
#     for p in parameters:
#         p.data += -0.1 * p.grad
    


# # test
# test_input = torch.tensor([1, 2, 3, 4]).float()
# h = torch.tanh(test_input @ W1 + b1)
# logits = h @ W2 + b2

# probs = F.softmax(logits, dim=0)
# print(probs)
# pred = torch.argmax(logits)
# print(pred.item())
    
    
    

