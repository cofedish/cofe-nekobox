//go:build windows

package main

import (
	"fmt"
	"os/exec"
	"strings"
)

func isPidRunning(pid int) bool {
	if pid <= 0 {
		return false
	}
	out, err := exec.Command("tasklist", "/FI", fmt.Sprintf("PID eq %d", pid), "/FO", "CSV", "/NH").CombinedOutput()
	if err != nil {
		return false
	}
	line := strings.ToLower(strings.TrimSpace(string(out)))
	if line == "" || strings.Contains(line, "no tasks are running") {
		return false
	}
	needle := fmt.Sprintf(",\"%d\",", pid)
	return strings.Contains(line, needle)
}
