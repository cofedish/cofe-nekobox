package main

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"io"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
	"time"

	"github.com/codeclysm/extract"
)

type autoUpdateConfig struct {
	mode           string
	pid            int
	installDir     string
	archivePath    string
	appExe         string
	backupTag      string
	appImagePath   string
	downloadedPath string
	debPath        string
}

func runAutoUpdateFromArgs() error {
	fs := flag.NewFlagSet("cofebox-updater", flag.ContinueOnError)
	cfg := autoUpdateConfig{}
	fs.StringVar(&cfg.mode, "mode", "", "update mode: windows|appimage|deb")
	fs.IntVar(&cfg.pid, "pid", 0, "pid of running process")
	fs.StringVar(&cfg.installDir, "install-dir", "", "target install dir")
	fs.StringVar(&cfg.archivePath, "archive-path", "", "downloaded archive path")
	fs.StringVar(&cfg.appExe, "app-exe", "", "executable to relaunch")
	fs.StringVar(&cfg.backupTag, "backup-tag", "", "backup version tag")
	fs.StringVar(&cfg.appImagePath, "appimage-path", "", "current appimage path")
	fs.StringVar(&cfg.downloadedPath, "downloaded-path", "", "downloaded appimage path")
	fs.StringVar(&cfg.debPath, "deb-path", "", "downloaded deb path")
	if err := fs.Parse(os.Args[1:]); err != nil {
		return err
	}

	if err := waitForPidExit(cfg.pid, 90*time.Second); err != nil {
		return err
	}

	switch strings.ToLower(strings.TrimSpace(cfg.mode)) {
	case "windows":
		return updateWindowsZip(cfg)
	case "appimage":
		return updateAppImage(cfg)
	case "deb":
		return installDeb(cfg)
	default:
		return fmt.Errorf("unknown mode: %q", cfg.mode)
	}
}

func waitForPidExit(pid int, timeout time.Duration) error {
	if pid <= 0 {
		return nil
	}
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		if !isPidRunning(pid) {
			return nil
		}
		time.Sleep(500 * time.Millisecond)
	}
	return fmt.Errorf("process %d is still running after %s", pid, timeout)
}

func updateWindowsZip(cfg autoUpdateConfig) error {
	if runtime.GOOS != "windows" {
		return errors.New("windows mode is only available on Windows")
	}
	if cfg.installDir == "" || cfg.archivePath == "" || cfg.appExe == "" {
		return errors.New("windows mode requires --install-dir, --archive-path and --app-exe")
	}
	if !fileExists(cfg.archivePath) {
		return fmt.Errorf("archive not found: %s", cfg.archivePath)
	}

	stagingRoot := filepath.Join(os.TempDir(), "cofebox-update", fmt.Sprintf("staging-%d", time.Now().UnixNano()))
	if err := os.MkdirAll(stagingRoot, 0755); err != nil {
		return err
	}
	defer os.RemoveAll(stagingRoot)

	if err := extractArchive(cfg.archivePath, stagingRoot); err != nil {
		return err
	}
	payloadRoot, err := detectPayloadRoot(stagingRoot)
	if err != nil {
		return err
	}

	backupName := ".cofebox-backup-" + time.Now().Format("20060102-150405")
	if strings.TrimSpace(cfg.backupTag) != "" {
		backupName += "-" + strings.TrimSpace(cfg.backupTag)
	}
	backupDir := filepath.Join(filepath.Dir(cfg.installDir), backupName)
	if err = copyDir(cfg.installDir, backupDir, nil); err != nil {
		return fmt.Errorf("backup failed: %w", err)
	}

	skip := map[string]bool{
		strings.ToLower(filepath.Base(os.Args[0])): true, // avoid overwriting running updater binary
	}
	if err = copyDir(payloadRoot, cfg.installDir, skip); err != nil {
		_ = copyDir(backupDir, cfg.installDir, nil)
		return fmt.Errorf("install failed and rollback attempted: %w", err)
	}

	log.Printf("windows update installed from %s to %s", payloadRoot, cfg.installDir)
	return exec.Command(cfg.appExe).Start()
}

func updateAppImage(cfg autoUpdateConfig) error {
	if runtime.GOOS != "linux" {
		return errors.New("appimage mode is only available on Linux")
	}
	if cfg.appImagePath == "" || cfg.downloadedPath == "" {
		return errors.New("appimage mode requires --appimage-path and --downloaded-path")
	}
	if !fileExists(cfg.downloadedPath) {
		return fmt.Errorf("downloaded appimage not found: %s", cfg.downloadedPath)
	}
	targetInfo, err := os.Stat(cfg.appImagePath)
	if err != nil {
		return fmt.Errorf("cannot access target appimage: %w", err)
	}
	if targetInfo.Mode()&0200 == 0 {
		return fmt.Errorf("target appimage is not writable: %s", cfg.appImagePath)
	}

	backupPath := cfg.appImagePath + ".bak"
	_ = os.Remove(backupPath)
	if err = os.Rename(cfg.appImagePath, backupPath); err != nil {
		return fmt.Errorf("backup rename failed: %w", err)
	}
	if err = copyFile(cfg.downloadedPath, cfg.appImagePath, 0755); err != nil {
		_ = os.Rename(backupPath, cfg.appImagePath)
		return fmt.Errorf("replace appimage failed: %w", err)
	}
	if err = os.Chmod(cfg.appImagePath, 0755); err != nil {
		_ = os.Remove(cfg.appImagePath)
		_ = os.Rename(backupPath, cfg.appImagePath)
		return fmt.Errorf("chmod appimage failed: %w", err)
	}

	log.Printf("appimage updated: %s", cfg.appImagePath)
	if err = exec.Command(cfg.appImagePath).Start(); err != nil {
		_ = os.Remove(cfg.appImagePath)
		_ = os.Rename(backupPath, cfg.appImagePath)
		return fmt.Errorf("failed to relaunch appimage: %w", err)
	}
	return nil
}

func installDeb(cfg autoUpdateConfig) error {
	if runtime.GOOS != "linux" {
		return errors.New("deb mode is only available on Linux")
	}
	if cfg.debPath == "" {
		return errors.New("deb mode requires --deb-path")
	}
	if !fileExists(cfg.debPath) {
		return fmt.Errorf("deb package not found: %s", cfg.debPath)
	}

	cmd := exec.Command("pkexec", "apt", "install", "-y", cfg.debPath)
	output, err := cmd.CombinedOutput()
	if err != nil {
		return fmt.Errorf("deb install failed: %w\n%s", err, string(output))
	}
	log.Printf("deb installed: %s", cfg.debPath)
	return nil
}

func extractArchive(archivePath, targetDir string) error {
	f, err := os.Open(archivePath)
	if err != nil {
		return err
	}
	defer f.Close()

	if strings.HasSuffix(strings.ToLower(archivePath), ".zip") {
		return extract.Zip(context.Background(), f, targetDir, nil)
	}
	if strings.HasSuffix(strings.ToLower(archivePath), ".tar.gz") {
		return extract.Gz(context.Background(), f, targetDir, nil)
	}
	return fmt.Errorf("unsupported archive format: %s", archivePath)
}

func detectPayloadRoot(stagingRoot string) (string, error) {
	candidate := filepath.Join(stagingRoot, "nekoray")
	if stat, err := os.Stat(candidate); err == nil && stat.IsDir() {
		return candidate, nil
	}

	entries, err := os.ReadDir(stagingRoot)
	if err != nil {
		return "", err
	}
	if len(entries) == 1 && entries[0].IsDir() {
		return filepath.Join(stagingRoot, entries[0].Name()), nil
	}
	return stagingRoot, nil
}

func copyDir(src, dst string, skipBase map[string]bool) error {
	src = filepath.Clean(src)
	dst = filepath.Clean(dst)
	return filepath.Walk(src, func(path string, info os.FileInfo, walkErr error) error {
		if walkErr != nil {
			return walkErr
		}
		rel, err := filepath.Rel(src, path)
		if err != nil {
			return err
		}
		if rel == "." {
			return os.MkdirAll(dst, info.Mode())
		}
		target := filepath.Join(dst, rel)

		if skipBase != nil && !info.IsDir() {
			if skipBase[strings.ToLower(filepath.Base(path))] {
				return nil
			}
		}

		if info.Mode()&os.ModeSymlink != 0 {
			linkTarget, err := os.Readlink(path)
			if err != nil {
				return err
			}
			_ = os.Remove(target)
			return os.Symlink(linkTarget, target)
		}
		if info.IsDir() {
			return os.MkdirAll(target, info.Mode())
		}
		return copyFile(path, target, info.Mode())
	})
}

func copyFile(src, dst string, mode os.FileMode) error {
	in, err := os.Open(src)
	if err != nil {
		return err
	}
	defer in.Close()

	if err = os.MkdirAll(filepath.Dir(dst), 0755); err != nil {
		return err
	}

	tmp := dst + ".tmp-cofebox"
	out, err := os.OpenFile(tmp, os.O_CREATE|os.O_TRUNC|os.O_WRONLY, mode)
	if err != nil {
		return err
	}
	if _, err = io.Copy(out, in); err != nil {
		out.Close()
		_ = os.Remove(tmp)
		return err
	}
	if err = out.Close(); err != nil {
		_ = os.Remove(tmp)
		return err
	}
	_ = os.Remove(dst)
	return os.Rename(tmp, dst)
}
