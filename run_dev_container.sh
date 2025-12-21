docker run --rm -it \
  -v "$PWD":/workspace \
  -w /workspace \
  --network host \
  stream-pipeline-dev bash
