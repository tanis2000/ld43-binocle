((nil . ((eval . (setq
                  projectile-project-test-cmd #'helm-ctest
                  projectile-project-compilation-cmd #'helm-make-projectile
                  projectile-project-compilation-dir "build"
                  helm-make-build-dir (projectile-compilation-dir)
                  helm-ctest-dir (projectile-compilation-dir)
                  ))
         (cmake-ide-project-dir . "~/Documents/ld43-binocle")
         (cmake-ide-build-dir . "~/Documents/ld43-binocle/build")
         (cmake-ide-cmake-opts . "-DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON")
         (projectile-project-name . "LD43 Binocle Project")
         (projectile-project-run-cmd . "build/ld43-binocle.app/Contents/MacOS/ld43-binocle")
         (projectile-project-configure-cmd . "cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON")
         (helm-make-arguments . "-j8")
  )))
