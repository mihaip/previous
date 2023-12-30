# Previous NeXT Emulator

GitHub mirror of the Previous SVN repository at https://svn.code.sf.net/p/previous/code/, currently from the `branch_softfloat` branch.

The intent of this repository to maintain a fork of Previous, modified to be compiled to WebAssembly and run in the browser. Part of the [Infinite Mac project](https://github.com/mihaip/infinite-mac).

See readme.previous.txt for the original README.

## Upstream configuration

The SVN mirror is configured via [git svn](https://git-scm.com/docs/git-svn). An `upstream` branch is configured to track the `branch_softfloat` branch in the Previous SVN repository:

```
git config --add svn-remote.branch_softfloat.url https://svn.code.sf.net/p/previous/code/branches/branch_softfloat
git config --add svn-remote.branch_softfloat.fetch :refs/remotes/branch_softfloat
git svn fetch branch_softfloat
git checkout -b upstream branch_softfloat
git svn rebase branch_softfloat
```

To update this repo's `main` branch, switch to it and run:

```
git rebase --committer-date-is-author-date upstream
```
