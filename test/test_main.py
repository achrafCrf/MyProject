import pytest
from unittest.mock import patch

@patch("projet.main.get_distance")
@patch("Adafruit_BBIO.GPIO")
def test_led_on_when_close(mock_GPIO, mock_get_distance):
    mock_get_distance.return_value = 5
    from projet.main import main
    with patch("builtins.input", side_effect=KeyboardInterrupt):
        try:
            main()
        except KeyboardInterrupt:
            pass
    assert mock_GPIO.output.called

@patch("projet.main.get_distance")
@patch("Adafruit_BBIO.GPIO")
def test_led_off_when_far(mock_GPIO, mock_get_distance):
    mock_get_distance.return_value = 20
    from projet.main import main
    with patch("builtins.input", side_effect=KeyboardInterrupt):
        try:
            main()
        except KeyboardInterrupt:
            pass
    assert mock_GPIO.output.called
