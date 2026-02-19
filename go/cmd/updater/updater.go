package main

import (
	"context"
	"fmt"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
	"time"

	"github.com/codeclysm/extract"
)

// runLegacyUpdaterEntry keeps compatibility with the historical updater behavior.
func runLegacyUpdaterEntry(exeBase string, args []string) error {
	if runtime.GOOS == "windows" {
		if strings.HasPrefix(exeBase, "updater.old") {
			time.Sleep(time.Second)
			if err := runLegacyUpdater(args); err != nil {
				return err
			}
			_ = exec.Command("./nekobox.exe").Start()
			return nil
		}
		if err := copyFile("./updater.exe", "./updater.old", 0644); err != nil {
			return err
		}
		return exec.Command("./updater.old", args...).Start()
	}

	if err := runLegacyUpdater(args); err != nil {
		return err
	}
	if os.Getenv("NKR_FROM_LAUNCHER") == "1" {
		Launcher()
		return nil
	}
	return exec.Command("./nekobox").Start()
}

func runLegacyUpdater(args []string) error {
	preCleanup := func() {
		if runtime.GOOS == "linux" {
			_ = os.RemoveAll("./usr")
		}
		_ = os.RemoveAll("./nekoray_update")
	}

	updatePackagePath := ""
	if len(args) >= 1 && fileExists(args[0]) {
		updatePackagePath = args[0]
	} else if fileExists("./nekoray.zip") {
		updatePackagePath = "./nekoray.zip"
	} else if fileExists("./nekoray.tar.gz") {
		updatePackagePath = "./nekoray.tar.gz"
	}
	if updatePackagePath == "" {
		return fmt.Errorf("no legacy update package found")
	}

	log.Printf("legacy update from %s", updatePackagePath)
	preCleanup()

	if strings.HasSuffix(updatePackagePath, ".zip") {
		f, err := os.Open(updatePackagePath)
		if err != nil {
			return err
		}
		defer f.Close()
		if err = extract.Zip(context.Background(), f, "./nekoray_update", nil); err != nil {
			return err
		}
	} else if strings.HasSuffix(updatePackagePath, ".tar.gz") {
		f, err := os.Open(updatePackagePath)
		if err != nil {
			return err
		}
		defer f.Close()
		if err = extract.Gz(context.Background(), f, "./nekoray_update", nil); err != nil {
			return err
		}
	} else {
		return fmt.Errorf("unsupported legacy update package: %s", updatePackagePath)
	}

	removeAllGlob("./*.dll")
	removeAllGlob("./*.dmp")

	if err := moveTree("./nekoray_update/nekoray", "./"); err != nil {
		return err
	}

	_ = os.RemoveAll("./nekoray_update")
	_ = os.RemoveAll("./nekoray.zip")
	_ = os.RemoveAll("./nekoray.tar.gz")
	_ = os.Remove("./nekoray.exe")
	_ = os.Remove("./nekoray.png")
	_ = os.Remove("./nekoray_core.exe")
	return nil
}

func removeAllGlob(glob string) {
	files, _ := filepath.Glob(glob)
	for _, f := range files {
		_ = os.Remove(f)
	}
}

func moveTree(src, dst string) error {
	info, err := os.Stat(src)
	if err != nil {
		return err
	}
	if info.IsDir() {
		entries, err := os.ReadDir(src)
		if err != nil {
			return err
		}
		for _, entry := range entries {
			if err = moveTree(filepath.Join(src, entry.Name()), filepath.Join(dst, entry.Name())); err != nil {
				return err
			}
		}
		return nil
	}
	if err = os.MkdirAll(filepath.Dir(dst), 0755); err != nil {
		return err
	}
	return os.Rename(src, dst)
}

func fileExists(path string) bool {
	_, err := os.Stat(path)
	return err == nil
}
