# Edge AI Movement Tracker: Fitting a Neural Network into a tiny device

Welcome to the companion repository for our video: **https://youtu.be/7ECAZgulMkc**.

This repo contains the training scripts, datasets, and high-level logic used to build an ultra-lightweight Edge AI model. While our final production board is designed to monitor cow health on a farm, this repository focuses on the human-wearable prototype we built to test the underlying engineering. We are using **Embedded Machine Learning** techniques and **TensorFlow Lite for Microcontrollers (TFLM)** to track specific movements like walking, jumping jacks, and drinking coffee directly on the edge device.

---

## Repository Contents

* **`EdgeAI_Training_Script.ipynb`**: The core Jupyter Notebook containing the data preprocessing, model architecture, training loop, and quantization steps.
* **`movement_dataset/`**: The raw SQLite database files containing the captured movement data:

```text
movement_dataset/
├── Drinking_Coffee.db
├── Idle.db
├── Jumping_Jacks.db
└── Like_and_Subscribe.db
```

---

## Project Overview

Running machine learning models on severely constrained wearables requires aggressive optimization. We couldn't stream raw sensor data to the cloud due to battery limits, so we had to bring the "brain" directly to the edge.

Here is a breakdown of the pipeline detailed in the script:

* **Accelerometer Data Processing:** Raw 3-axis movement data is broadcasted via a Nordic Semiconductor BLE chip, captured by a custom Go application on a PC, and logged into an SQLite database.
* **Preprocessing:** The continuous data stream is sliced into rigid, overlapping 3-second windows. We use a strict time-based split to separate training and testing data, effectively preventing data leakage.
* **Time-Series Classification:** To handle the sequential data efficiently, we bypass heavy, stateful models like RNNs or LSTMs. Instead, we use a highly constrained **1D CNN (Convolutional Neural Network)**. It is stateless, focusing purely on the geometric "shape" of the movement in the current window.
* **Quantization (INT8):** To fit the model onto the microcontroller's limited memory, we use the TensorFlow Lite Converter inside `EdgeAI_Training_Script.ipynb` to quantize the model weights. We map hyper-precise Float32 decimals down to 8-bit integers (INT8), saving critical RAM and flash memory space.
* **Deployment:** The resulting `.tflite` model is deployed natively on Zephyr RTOS using TFLM.

---

## Useful Links and Resources

If you are building your own TFLM project on Zephyr or Nordic hardware, these links will save you hours of debugging and hard faults:

* **Companion YouTube Video:** https://youtu.be/7ECAZgulMkc
* **TFLite Micro Repository:** https://github.com/tensorflow/tflite-micro
* **TFLite Operator Compatibility List:** https://ai.google.dev/edge/litert/models/ops_compatibility
* **Micro Mutable Op Resolver:** https://github.com/tensorflow/tflite-micro/blob/main/tensorflow/lite/micro/micro_mutable_op_resolver.h
* **Nordic TFLite-Micro Hello World Sample:** https://docs.nordicsemi.com/bundle/ncs-2.5.2/page/zephyr/samples/modules/tflite-micro/hello_world/README.html
* **TFLite Micro on Zephyr (In-Depth Guide):** https://www.youtube.com/watch?v=vMDxmEwu51M
* **Zephyr RTOS Issue #40827 (TFLite integration):** https://github.com/zephyrproject-rtos/zephyr/issues/40827