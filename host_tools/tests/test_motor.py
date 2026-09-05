import pytest

from openflightcomputer.workflows.motor import (
    CLEANUP_ZERO_WRITES,
    execute_motor_test,
    validate_motor_test,
)


class FakeClock:
    def __init__(self):
        self.now = 0.0

    def monotonic(self):
        return self.now

    def sleep(self, duration):
        self.now += duration


class FakeClient:
    def __init__(self, state="ARMED", fail_on_positive=False):
        self.state = state
        self.fail_on_positive = fail_on_positive
        self.requests = []

    def request(self, command, *, parameters=None, timeout_seconds=2.0):
        self.requests.append((command, parameters))
        if command == "status":
            return {"state": self.state}
        if command == "motor_test" and parameters["throttle"] > 0 and self.fail_on_positive:
            raise KeyboardInterrupt
        return {"ok": True}


@pytest.mark.parametrize(
    ("motor", "throttle", "duration"),
    [
        (2, 0.02, 0.25),
        (1, 0.001, 0.25),
        (1, 0.100001, 0.25),
        (1, 0.02, 1.001),
    ],
)
def test_invalid_motor_tests_are_rejected(motor, throttle, duration):
    with pytest.raises(ValueError):
        validate_motor_test(motor, throttle, duration)


def test_workflow_prepares_refreshes_stops_and_disarms():
    clock = FakeClock()
    client = FakeClient()
    sent = execute_motor_test(
        client, 1, 0.02, 0.05, monotonic=clock.monotonic, sleeper=clock.sleep
    )
    assert sent >= 3
    motor_requests = [
        parameters
        for command, parameters in client.requests
        if command == "motor_test"
    ]
    first_positive = next(
        index for index, item in enumerate(motor_requests) if item["throttle"] > 0
    )
    assert first_positive >= 50
    assert all(item["motor"] == 1 for item in motor_requests)
    assert all(item["throttle"] == 0.0 for item in motor_requests[-CLEANUP_ZERO_WRITES:])
    assert client.requests[-1] == ("disarm", None)


def test_workflow_refuses_to_arm_implicitly():
    client = FakeClient(state="DISARMED")
    with pytest.raises(ValueError, match="already be ARMED"):
        execute_motor_test(client, 1, 0.02, 0.05)
    assert client.requests == [("status", None)]


def test_interrupt_still_sends_zero_and_disarms():
    clock = FakeClock()
    client = FakeClient(fail_on_positive=True)
    with pytest.raises(KeyboardInterrupt):
        execute_motor_test(
            client, 1, 0.02, 0.05, monotonic=clock.monotonic, sleeper=clock.sleep
        )
    assert client.requests[-1] == ("disarm", None)
    assert all(
        parameters["throttle"] == 0.0
        for command, parameters in client.requests[-(CLEANUP_ZERO_WRITES + 1):-1]
        if command == "motor_test"
    )
