import logging
import sys


def log_to_file(f, level=logging.INFO):
    handler = logging.StreamHandler(f)
    formatter = logging.Formatter('%(asctime)s %(levelname)s [%(name)s]: %(message)s')
    handler.setFormatter(formatter)
    logging.root.addHandler(handler)
    logging.root.setLevel(level)
