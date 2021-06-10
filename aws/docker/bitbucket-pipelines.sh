#!/usr/bin/env bash

# turn on strict bash
set -euo pipefail

organization=gamichaelmoore
repository=ginarn-base-ubuntu-18.04-test

docker build -t "$organization/$repository" --build-arg AWS_SECRET_KEY="$AWS_SECRET_ACCESS_KEY" --build-arg AWS_ACCESS_KEY="$AWS_ACCESS_KEY_ID" -f Dockerfile .
#docker build -t "$organization/$repository" -f Dockerfile .
docker login -u "$DOCKER_HUB_USERNAME" -p "$DOCKER_HUB_PASSWORD"
docker push "$organization/$repository"
