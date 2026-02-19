package main

import (
	"log"
	"os"
	"path/filepath"
	"strings"
)

func main() {
	exePath, err := os.Executable()
	if err != nil {
		log.Fatalf("resolve executable: %v", err)
	}

	wd := filepath.Dir(exePath)
	_ = os.Chdir(wd)

	exeBase := strings.ToLower(filepath.Base(os.Args[0]))
	log.Printf("exe=%s dir=%s args=%v", exeBase, wd, os.Args[1:])

	if hasArg("--mode") {
		if err := runAutoUpdateFromArgs(); err != nil {
			log.Printf("auto-update failed: %v", err)
			MessageBoxPlain("CofeBox Updater", "Update failed:\n\n"+err.Error())
			os.Exit(1)
		}
		return
	}

	if strings.HasPrefix(exeBase, "launcher") {
		Launcher()
		return
	}

	// Legacy entrypoint compatibility.
	if strings.HasPrefix(exeBase, "updater") {
		if err := runLegacyUpdaterEntry(exeBase, os.Args[1:]); err != nil {
			log.Printf("legacy updater failed: %v", err)
			MessageBoxPlain("CofeBox Updater", "Update failed:\n\n"+err.Error())
			os.Exit(1)
		}
		return
	}

	log.Fatalf("unsupported executable name: %s", exeBase)
}

func hasArg(name string) bool {
	for _, arg := range os.Args[1:] {
		if arg == name {
			return true
		}
	}
	return false
}
