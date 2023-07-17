class PickleError(Exception):
    """Something went wrong in a Pickle program."""


class ParseError(PickleError):
    """The Pickle parser encountered something it doesn't know how to parse."""


class ParseFail(Exception):
    """Signal used to indicate that parsing failed."""
