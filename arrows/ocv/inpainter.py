# This file is part of KWIVER, and is distributed under the
# OSI-approved BSD 3-Clause License. See top-level LICENSE file or
# https://github.com/Kitware/kwiver/blob/master/LICENSE for details.

from __future__ import division, print_function

from vital.algo import ImageFilter

# Taken from
# https://github.com/Kitware/burn-out/blob/master/library/object_detectors/pixel_annotation_loader.cxx
COLOR_ID_MAP = {
    0: np.array([[[0, 0, 0]]], dtype=np.uint8),  # BACKGROUND_ID
    1: np.array([[[0, 255, 0]]], dtype=np.uint8),  # GREEN_PIXEL_ID
    3: np.array([[[255, 255, 0]]], dtype=np.uint8),  # YELLOW_PIXEL_ID
    4: np.array([[[0, 0, 255]]], dtype=np.uint8),  # BLUE_PIXEL_ID
    5: np.array([[[255, 0, 0]]], dtype=np.uint8),  # RED_PIXEL_ID
    6: np.array([[[255, 0, 255]]], dtype=np.uint8),  # PINK_PIXEL_ID
    # The GREEN_PIXEL_BORDER_ID enum is excluded for now
}


class Inpainter(ImageFilter):
    """
    Implementation of image inpainting
    """

    def __init__(self):
        ImageFilter.__init__(self)

    def get_configuration(self):
        # Inherit from the base class
        cfg = super(ImageFilter, self).get_configuration()

        # cfg.set_value("identifier", self._identifier)

        return cfg

    def set_configuration(self, cfg_in):
        cfg = self.get_configuration()
        cfg.merge_config(cfg_in)

        self._generate_empty_images = str(cfg.get_value("generate_empty_images"))

        from vital.modules.modules import load_known_modules

        load_known_modules()

        return True

    def check_configuration(self, cfg):
        return True

    def interupt_handler(self):
        # Give the subprocess five seconds to exit cleanly
        if self.proc is not None:
            self.proc.send_signal(signal.SIGINT)
            timeout = 0
            while self.proc.poll() is None:
                time.sleep(0.1)
                timeout += 0.1
                if timeout > 5:
                    self.proc.kill()
                    break
        sys.exit(0)


def __vital_algorithm_register__():
    from vital.algo import algorithm_factory

    # Register Algorithm
    implementation_name = "ocv_inpainter"

    if algorithm_factory.has_algorithm_impl_name(
        BurnOutTrainer.static_type_name(), implementation_name
    ):
        return

    algorithm_factory.add_algorithm(
        implementation_name,
        "Inpainting",
        Inpainter,
    )

    algorithm_factory.mark_algorithm_as_loaded(implementation_name)
