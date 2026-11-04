using System;
using System.IO;

namespace Server.Game.Navigation
{
    public static class NavigationContentPath
    {
        public const string DefaultConfigurationPath = "Navigation/navigation-maps.json";

        public static string GetDefaultContentRoot()
        {
            return Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "Content"));
        }

        public static bool TryResolveContentRoot(string absoluteOverride, out string contentRoot, out string error)
        {
            contentRoot = null;
            error = null;

            try
            {
                if (string.IsNullOrWhiteSpace(absoluteOverride))
                {
                    contentRoot = GetDefaultContentRoot();

                    return true;
                }

                if (!Path.IsPathRooted(absoluteOverride))
                {
                    error = "The development Content Root override must be an absolute path.";

                    return false;
                }

                contentRoot = Path.GetFullPath(absoluteOverride);

                return true;
            }
            catch (Exception exception) when (exception is ArgumentException || exception is NotSupportedException ||
                                              exception is PathTooLongException)
            {
                error = "Invalid Content Root: " + exception.Message;

                return false;
            }
        }

        public static bool TryResolveAssetPath(string contentRoot, string relativePath, out string fullPath,
                                               out string error)
        {
            fullPath = null;
            error = null;

            if (string.IsNullOrWhiteSpace(contentRoot))
            {
                error = "Content Root is empty.";

                return false;
            }

            if (string.IsNullOrWhiteSpace(relativePath))
            {
                error = "Content-relative path is empty.";

                return false;
            }

            if (Path.IsPathRooted(relativePath) || relativePath.IndexOf(':') >= 0 ||
                relativePath.StartsWith("\\\\", StringComparison.Ordinal) ||
                relativePath.StartsWith("//", StringComparison.Ordinal))
            {
                error = "Absolute, drive, and UNC content paths are not allowed.";

                return false;
            }

            string[] segments = relativePath.Split(new[] { '/', '\\' }, StringSplitOptions.RemoveEmptyEntries);

            foreach (string segment in segments)
            {
                if (segment == "..")
                {
                    error = "Parent-directory traversal is not allowed in content paths.";

                    return false;
                }
            }

            try
            {
                string normalizedRoot = Path.GetFullPath(contentRoot);
                string combined = Path.GetFullPath(Path.Combine(normalizedRoot, relativePath));
                string rootPrefix =
                    normalizedRoot.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar) +
                    Path.DirectorySeparatorChar;

                if (!combined.StartsWith(rootPrefix, StringComparison.OrdinalIgnoreCase))
                {
                    error = "Resolved content path escapes the Content Root.";

                    return false;
                }

                fullPath = combined;

                return true;
            }
            catch (Exception exception) when (exception is ArgumentException || exception is NotSupportedException ||
                                              exception is PathTooLongException)
            {
                error = "Invalid content path: " + exception.Message;

                return false;
            }
        }
    }
}
