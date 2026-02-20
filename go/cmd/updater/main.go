package main

import (
	"log"
	"os"
	"path/filepath"
	"strings"
)

func main() {
	// update & launcher
	exe, err := os.Executable()
	if err != nil {
		panic(err.Error())
	}

	wd := filepath.Dir(exe)
	os.Chdir(wd)
	exe = filepath.Base(os.Args[0])
	log.Println("exe:", exe, "exe dir:", wd)
	exeLower := strings.ToLower(exe)

	if strings.HasPrefix(exeLower, "updater") {
		if hasAutoUpdateModeArg(os.Args[1:]) {
			if err := runAutoUpdateFromArgs(); err != nil {
				log.Printf("auto update failed: %v", err)
				os.Exit(1)
			}
			return
		}
		if err := runLegacyUpdaterEntry(exeLower, os.Args[1:]); err != nil {
			log.Printf("legacy updater failed: %v", err)
			os.Exit(1)
		}
		return
	} else if strings.HasPrefix(exeLower, "launcher") {
		Launcher()
		return
	}
	log.Fatalf("wrong name")
}

func hasAutoUpdateModeArg(args []string) bool {
	for i, arg := range args {
		if arg == "--mode" {
			return i+1 < len(args) && strings.TrimSpace(args[i+1]) != ""
		}
		if strings.HasPrefix(arg, "--mode=") {
			return strings.TrimSpace(strings.TrimPrefix(arg, "--mode=")) != ""
		}
	}
	return false
}
