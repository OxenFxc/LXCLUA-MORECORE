# Contributing to LXCLUA-NCore / 贡献指南

[English](#english) | [中文](#中文)

---

## English

Thank you for your interest in contributing to LXCLUA-NCore!

### How to Contribute

#### Reporting Bugs

1. Check if the issue already exists in [Issues](../../issues)
2. Create a new issue with:
   - Clear title describing the problem
   - Steps to reproduce
   - Expected vs actual behavior
   - Environment info (OS, compiler version)

#### Suggesting Features

1. Open an issue with the `enhancement` label
2. Describe the feature and its use case
3. Discuss implementation approach if possible

#### Submitting Code

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/your-feature`
3. Make your changes
4. Ensure code compiles without warnings
5. Test your changes
6. Commit with clear messages
7. Push and create a Pull Request

### Code Style

- Use C23 standard
- Follow existing code formatting
- Add comments for complex logic
- Keep functions focused and concise

### Build & Test

```bash
make clean
make
make test
```

---

## 中文

感谢您对 LXCLUA-NCore 项目的关注！

### 如何贡献

#### 报告 Bug

1. 先在 [Issues](../../issues) 中检查是否已存在相同问题
2. 创建新 Issue，包含：
   - 清晰描述问题的标题
   - 复现步骤
   - 预期行为与实际行为
   - 环境信息（操作系统、编译器版本）

#### 功能建议

1. 创建带有 `enhancement` 标签的 Issue
2. 描述功能及其使用场景
3. 如可能，讨论实现方案

#### 提交代码

1. Fork 本仓库
2. 创建功能分支：`git checkout -b feature/your-feature`
3. 进行修改
4. 确保代码编译无警告
5. 测试你的修改
6. 提交清晰的 commit 信息
7. Push 并创建 Pull Request

### 代码风格

- 使用 C23 标准
- 遵循现有代码格式
- 为复杂逻辑添加注释
- 保持函数专注且简洁

### 构建与测试

```bash
make clean
make
make test
```
