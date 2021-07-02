#!/bin/bash

set -euo pipefail

cd /ginan
conda activate gn37
python scripts/download_examples.py
cd /ginan/examples
pod -y ex21_pod_fit_gps.yaml
pod -y ex22_pod_fit_gnss.yaml
pod -y ex23_pod_prd_gps.yaml
pod -y ex24_pod_ic_gps.yaml
pod -y ex25_pod_fit_gps.yaml
pod -y ex26_pod_fit_meo.yaml
