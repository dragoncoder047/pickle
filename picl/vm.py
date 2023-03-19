
class VM:
    "Main PICKLE VM that manages global state."

    def __init__(self):
        self.global_scope = None

    def parse(self, code):
        "Parse the code into a PICKLE abstract syntax tree."
        raise NotImplementedError
