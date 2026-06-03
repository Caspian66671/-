import os
from pathlib import Path

import torch
from torch import nn
from torch.utils.data import Dataset, DataLoader
from esp_ppq.api import espdl_quantize_torch


ROOT = Path(__file__).resolve().parents[1]
MODEL_DIR = ROOT / "main" / "models"
ESPDL_MODEL_PATH = MODEL_DIR / "workbuddy_advisor.espdl"
TARGET = "esp32p4"
NUM_OF_BITS = 8
DEVICE = "cpu"


class AdvisorNet(nn.Module):
    def __init__(self):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(8, 16),
            nn.ReLU(),
            nn.Linear(16, 12),
        )

    def forward(self, x):
        return self.net(x)


class FeatureDataset(Dataset):
    def __init__(self, features):
        self.features = features.float()

    def __len__(self):
        return self.features.shape[0]

    def __getitem__(self, index):
        return self.features[index]


def make_features(hour, rest_day, rain, hot, cold, sunny, cloudy, holiday):
    return [
        max(-1.0, min(1.0, (hour - 12) / 12.0)),
        1.0 if rest_day else -1.0,
        1.0 if rain else 0.0,
        1.0 if hot else -0.25,
        1.0 if cold else -0.25,
        0.75 if sunny else -0.25,
        0.75 if cloudy else -0.25,
        1.0 if holiday else -0.5,
    ]


def label_for(hour, rest_day, rain, hot):
    if rain:
        return 9   # UMBRELLA
    if rest_day:
        if 7 <= hour < 11:
            return 6   # EXERCISE
        if 11 <= hour < 14:
            return 1   # LUNCH
        if 14 <= hour < 18:
            return 7   # REST
        if 18 <= hour < 22:
            return 6   # EXERCISE
        return 8       # SLEEP
    if 6 <= hour < 9:
        return 0       # BREAKFAST
    if 9 <= hour < 11:
        return 3       # RESEARCH_FOCUS
    if 11 <= hour < 14:
        return 1       # LUNCH
    if 14 <= hour < 17:
        return 10 if hot else 4  # HYDRATE or PAPER_READING
    if 17 <= hour < 19:
        return 2       # DINNER
    if 19 <= hour < 22:
        return 5       # WRITE_THESIS
    if hour >= 22 or hour < 6:
        return 8       # SLEEP
    return 11          # PLAN


def build_dataset():
    xs = []
    ys = []
    for hour in range(24):
        for rest_day in (False, True):
            for rain in (False, True):
                for hot in (False, True):
                    for cold in (False, True):
                        for weather in ("sunny", "cloudy"):
                            sunny = weather == "sunny"
                            cloudy = weather == "cloudy"
                            holiday = rest_day
                            xs.append(make_features(hour, rest_day, rain, hot, cold, sunny, cloudy, holiday))
                            ys.append(label_for(hour, rest_day, rain, hot))
    return torch.tensor(xs, dtype=torch.float32), torch.tensor(ys, dtype=torch.long)


def train_model():
    torch.manual_seed(7)
    x, y = build_dataset()
    model = AdvisorNet().to(DEVICE)
    optimizer = torch.optim.Adam(model.parameters(), lr=0.025)
    loss_fn = nn.CrossEntropyLoss()

    for _ in range(450):
        logits = model(x)
        loss = loss_fn(logits, y)
        optimizer.zero_grad()
        loss.backward()
        optimizer.step()

    with torch.no_grad():
        acc = (model(x).argmax(dim=1) == y).float().mean().item()
    print(f"training accuracy: {acc:.3f}")
    return model.eval(), x


def export_model():
    MODEL_DIR.mkdir(parents=True, exist_ok=True)
    model, features = train_model()
    dataloader = DataLoader(FeatureDataset(features), batch_size=1, shuffle=False)
    test_input = features[0:1].clone()

    espdl_quantize_torch(
        model=model,
        espdl_export_file=str(ESPDL_MODEL_PATH),
        calib_dataloader=dataloader,
        calib_steps=min(64, len(dataloader)),
        input_shape=[1, 8],
        inputs=[test_input],
        target=TARGET,
        num_of_bits=NUM_OF_BITS,
        device=DEVICE,
        error_report=True,
        skip_export=False,
        export_test_values=True,
        verbose=1,
        dispatching_override=None,
    )
    print(f"exported: {ESPDL_MODEL_PATH}")


if __name__ == "__main__":
    os.environ.setdefault("CUDA_VISIBLE_DEVICES", "")
    export_model()
