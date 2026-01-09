# Contributing to ZZmazon

Thank you for your interest in contributing to ZZmazon! This document provides guidelines for contributing to the project.

## Ways to Contribute

There are many ways to contribute to ZZmazon:

- **Bug Reports**: Report bugs or issues you encounter
- **Feature Requests**: Suggest new features or improvements
- **Code Contributions**: Submit bug fixes or new features
- **Documentation**: Improve or translate documentation
- **AI Implementations**: Share your AI algorithms and strategies
- **Examples**: Add example code or tutorials

## Getting Started

1. Fork the repository
2. Clone your fork:
   ```bash
   git clone https://github.com/YOUR_USERNAME/The-Bot-of-Amazons.git
   cd The-Bot-of-Amazons
   ```
3. Create a new branch:
   ```bash
   git checkout -b feature/your-feature-name
   ```

## Development Setup

ZZmazon requires Python 3.7 or higher with no external dependencies.

```bash
# Install in development mode
pip install -e .

# Run tests
python test_zzmazon.py
```

## Code Style

- Follow PEP 8 guidelines for Python code
- Use meaningful variable and function names
- Add docstrings to classes and functions
- Keep functions focused and modular
- Add comments for complex logic

## Testing

Before submitting a pull request:

1. Run the existing test suite:
   ```bash
   python test_zzmazon.py
   ```

2. Test your changes manually:
   ```bash
   python zzmazon.py
   ```

3. If adding new features, add corresponding tests to `test_zzmazon.py`

## Submitting Changes

1. Commit your changes with clear, descriptive commit messages:
   ```bash
   git add .
   git commit -m "Add feature: description of your changes"
   ```

2. Push to your fork:
   ```bash
   git push origin feature/your-feature-name
   ```

3. Open a Pull Request on GitHub with:
   - Clear description of changes
   - Reference to any related issues
   - Screenshots if applicable (for UI changes)

## Pull Request Guidelines

- Keep changes focused and atomic
- Update documentation if needed
- Ensure all tests pass
- Follow the existing code style
- Be responsive to feedback and review comments

## AI Contributions

If contributing AI implementations:

- Document your algorithm and strategy
- Include performance metrics if available
- Add examples of your AI in action
- Consider adding to the `examples/` directory

## Questions?

Feel free to open an issue for:
- Questions about the codebase
- Clarification on contribution guidelines
- Discussion of potential features

## Code of Conduct

- Be respectful and inclusive
- Welcome newcomers and help them learn
- Focus on constructive feedback
- Assume good intentions

## License

By contributing to ZZmazon, you agree that your contributions will be licensed under the MIT License.

---

Thank you for contributing to ZZmazon! 🎮♛
