"""
Basic tests for xframes module functionality.
"""
import pytest
import xframes

def test_add():
    """Test the add function with various inputs."""
    assert xframes.add(1, 2) == 3
    assert xframes.add(-1, 1) == 0
    assert xframes.add(0, 0) == 0
    assert xframes.add(999999, 1) == 1000000