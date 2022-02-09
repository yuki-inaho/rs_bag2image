import cv2
import click
import numpy as np
from tqdm import tqdm, trange
from pathlib import Path, PosixPath
import shutil


def make_output_dir(output_image_dir_path: PosixPath, clean=False):
    if output_image_dir_path.exists() and clean:
        shutil.rmtree(str(output_image_dir_path))
    output_image_dir_path.mkdir(exist_ok=True)


def get_image_path_list(input_rgb_dir_pathlib):
    return np.sort([str(path) for path in input_rgb_dir_pathlib.glob("*") if path.suffix in [".jpg", ".png"]])


@click.command()
@click.option("--input-path", "-i", default="./rgb")
@click.option("--output-mp4-path", "-o", default="movie.mp4")
@click.option("--resize-rate", "-r", default=1.0)
@click.option("--frame-rate", "-f", default=10.0)
def main(input_path, output_mp4_path, resize_rate, frame_rate):
    input_image_list = get_image_path_list(Path(input_path))
    n_flames = len(input_image_list)
    print(f"Number of Frame: {n_flames}")

    # @TODO: reduce redundant code lines
    image = cv2.imread(input_image_list[0])
    image_resized = cv2.resize(image, None, fx=resize_rate, fy=resize_rate)
    size = (image_resized.shape[1], image_resized.shape[0])
    fmt = cv2.VideoWriter_fourcc("m", "p", "4", "v")
    writer = cv2.VideoWriter(output_mp4_path, fmt, frame_rate, size)

    for input_image_path in tqdm(input_image_list):
        image = cv2.imread(input_image_path)
        image_resized = cv2.resize(image, None, fx=resize_rate, fy=resize_rate)
        writer.write(image_resized)
        cv2.waitKey(10)
    writer.release()


if __name__ == "__main__":
    main()