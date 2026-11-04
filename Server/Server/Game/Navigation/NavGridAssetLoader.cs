using System;
using System.Collections.Generic;
using System.IO;
using System.Numerics;
using System.Security.Cryptography;
using System.Text;

namespace Server.Game.Navigation
{
    public enum NavGridLoadError
    {
        None,
        FileOpenFailed,
        FileTooLarge,
        TruncatedHeader,
        InvalidMagic,
        UnsupportedVersion,
        InvalidHeaderSize,
        MapIdTooLong,
        InvalidUtf8MapId,
        InvalidDimensions,
        CellCountTooLarge,
        InvalidCellSize,
        InvalidOrigin,
        InvalidAgentRadius,
        UnsupportedCellEncoding,
        InvalidCellDataLength,
        TruncatedCellData,
        TrailingData,
        UnknownCellValue,
        HashMismatch,
        HashFailure,
    }

    public sealed class NavGridLoadResult
    {
        private NavGridLoadResult(NavGridAsset asset, NavGridLoadError error, string message)
        {
            Asset = asset;
            Error = error;
            Message = message;
        }

        public NavGridAsset Asset { get; }
        public NavGridLoadError Error { get; }
        public string Message { get; }
        public bool Success => Error == NavGridLoadError.None;

        internal static NavGridLoadResult Ok(NavGridAsset asset) => new NavGridLoadResult(asset, NavGridLoadError.None,
                                                                                          string.Empty);
        internal static NavGridLoadResult Fail(NavGridLoadError error,
                                               string message) => new NavGridLoadResult(null, error, message);
    }

    public static class NavGridHash
    {
        public static byte[] ComputeSha256(byte[] bytes)
        {
            using (SHA256 sha256 = SHA256.Create()) return sha256.ComputeHash(bytes);
        }

        public static string ToHex(byte[] bytes)
        {
            var builder = new StringBuilder(bytes.Length * 2);

            foreach (byte value in bytes)
                builder.Append(value.ToString("x2"));

            return builder.ToString();
        }

        internal static bool FixedTimeEquals(byte[] left, byte[] right)
        {
            if (left == null || right == null || left.Length != right.Length)
                return false;
            int difference = 0;

            for (int i = 0; i < left.Length; ++i)
                difference |= left[i] ^ right[i];

            return difference == 0;
        }
    }

    public static class NavGridAssetLoader
    {
        public const uint FormatVersion = 1;
        public const uint CellEncodingBytePerCell = 1;
        public const uint MaximumMapIdBytes = 128;
        public const uint MaximumDimension = 4096;
        public const ulong MaximumCellCount = 4_194_304;
        private const ulong FixedHeaderBytes = 84;
        private const ulong MaximumFileBytes = FixedHeaderBytes + MaximumMapIdBytes + MaximumCellCount;
        private static readonly byte[] Magic = { (byte)'N', (byte)'V', (byte)'G', (byte)'1' };
        private static readonly UTF8Encoding StrictUtf8 = new UTF8Encoding(false, true);

        public static NavGridLoadResult Load(string path)
        {
            byte[] bytes;
            try
            {
                var info = new FileInfo(path);

                if (!info.Exists)
                    return NavGridLoadResult.Fail(NavGridLoadError.FileOpenFailed, "Cannot open NavGrid asset.");

                if ((ulong)info.Length > MaximumFileBytes)
                    return NavGridLoadResult.Fail(NavGridLoadError.FileTooLarge,
                                                  "NavGrid asset exceeds the V1 size limit.");
                bytes = File.ReadAllBytes(path);
            }
            catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException)
            {
                return NavGridLoadResult.Fail(NavGridLoadError.FileOpenFailed,
                                              "Cannot read NavGrid asset: " + exception.Message);
            }

            if (bytes.Length < 16)
                return Fail(NavGridLoadError.TruncatedHeader, "NavGrid header is truncated.");

            for (int i = 0; i < Magic.Length; ++i)
            {
                if (bytes[i] != Magic[i])
                    return Fail(NavGridLoadError.InvalidMagic, "NavGrid magic must be NVG1.");
            }

            int cursor = 4;

            if (!TryReadUInt32(bytes, ref cursor, out uint version) ||
                !TryReadUInt32(bytes, ref cursor, out uint headerSize) ||
                !TryReadUInt32(bytes, ref cursor, out uint mapIdLength))
            {
                return Fail(NavGridLoadError.TruncatedHeader, "NavGrid fixed header is truncated.");
            }

            if (version != FormatVersion)
                return Fail(NavGridLoadError.UnsupportedVersion, "Unsupported NavGrid format version.");

            if (mapIdLength > MaximumMapIdBytes)
                return Fail(NavGridLoadError.MapIdTooLong, "NavigationMapId exceeds 128 UTF-8 bytes.");

            ulong expectedHeaderSize = FixedHeaderBytes + mapIdLength;

            if (headerSize != expectedHeaderSize)
                return Fail(NavGridLoadError.InvalidHeaderSize, "HeaderSize does not match the V1 layout.");

            if ((ulong)bytes.Length < expectedHeaderSize)
                return Fail(NavGridLoadError.TruncatedHeader, "NavGrid variable header is truncated.");

            string mapId;
            try
            {
                mapId = StrictUtf8.GetString(bytes, cursor, (int)mapIdLength);
            }
            catch (DecoderFallbackException)
            {
                return Fail(NavGridLoadError.InvalidUtf8MapId, "NavigationMapId is not valid UTF-8.");
            }

            cursor += (int)mapIdLength;

            if (!TryReadUInt32(bytes, ref cursor, out uint width) ||
                !TryReadUInt32(bytes, ref cursor, out uint height) ||
                !TryReadSingle(bytes, ref cursor, out float cellSize) ||
                !TryReadSingle(bytes, ref cursor, out float originX) ||
                !TryReadSingle(bytes, ref cursor, out float originY) ||
                !TryReadSingle(bytes, ref cursor, out float originZ) ||
                !TryReadSingle(bytes, ref cursor, out float agentRadius) ||
                !TryReadUInt32(bytes, ref cursor, out uint cellEncoding) ||
                !TryReadUInt32(bytes, ref cursor, out uint cellDataLength))
            {
                return Fail(NavGridLoadError.TruncatedHeader, "NavGrid metadata is truncated.");
            }

            if (width == 0 || height == 0 || width > MaximumDimension || height > MaximumDimension)
                return Fail(NavGridLoadError.InvalidDimensions, "NavGrid dimensions are zero or exceed 4096.");
            ulong cellCount = (ulong)width * height;

            if (cellCount > MaximumCellCount)
                return Fail(NavGridLoadError.CellCountTooLarge, "NavGrid cell count exceeds the V1 limit.");

            if (!IsFinite(cellSize) || cellSize <= 0.0f)
                return Fail(NavGridLoadError.InvalidCellSize, "CellSize must be finite and positive.");

            if (!IsFinite(originX) || !IsFinite(originY) || !IsFinite(originZ))
                return Fail(NavGridLoadError.InvalidOrigin, "Origin must contain only finite values.");

            if (!IsFinite(agentRadius) || agentRadius < 0.0f)
                return Fail(NavGridLoadError.InvalidAgentRadius, "DefaultAgentRadius must be finite and non-negative.");

            if (cellEncoding != CellEncodingBytePerCell)
                return Fail(NavGridLoadError.UnsupportedCellEncoding, "Unsupported NavGrid cell encoding.");

            if (cellDataLength != cellCount)
                return Fail(NavGridLoadError.InvalidCellDataLength,
                            "CellDataLength must equal Width * Height for encoding 1.");

            if (cursor > bytes.Length || bytes.Length - cursor < 32)
                return Fail(NavGridLoadError.TruncatedHeader, "ContentHash is truncated.");
            var storedHash = new byte[32];
            Buffer.BlockCopy(bytes, cursor, storedHash, 0, storedHash.Length);
            cursor += storedHash.Length;

            if ((uint)cursor != headerSize)
                return Fail(NavGridLoadError.InvalidHeaderSize, "HeaderSize does not end after ContentHash.");

            ulong requiredLength = (ulong)cursor + cellDataLength;

            if ((ulong)bytes.Length < requiredLength)
                return Fail(NavGridLoadError.TruncatedCellData, "CellData is truncated.");

            if ((ulong)bytes.Length > requiredLength)
                return Fail(NavGridLoadError.TrailingData, "V1 rejects bytes after CellData.");

            var cells = new byte[(int)cellDataLength];
            Buffer.BlockCopy(bytes, cursor, cells, 0, cells.Length);

            foreach (byte cell in cells)
            {
                if (cell > (byte)NavCellType.Slow)
                    return Fail(NavGridLoadError.UnknownCellValue, "CellData contains an unknown V1 cell value.");
            }

            byte[] canonical;

            using (var stream = new MemoryStream())
            {
                WriteUInt32(stream, version);
                WriteUInt32(stream, mapIdLength);
                stream.Write(bytes, 16, (int)mapIdLength);
                WriteUInt32(stream, width);
                WriteUInt32(stream, height);
                WriteSingle(stream, cellSize);
                WriteSingle(stream, originX);
                WriteSingle(stream, originY);
                WriteSingle(stream, originZ);
                WriteSingle(stream, agentRadius);
                WriteUInt32(stream, cellEncoding);
                WriteUInt32(stream, cellDataLength);
                stream.Write(cells, 0, cells.Length);
                canonical = stream.ToArray();
            }

            byte[] computedHash;
            try
            {
                computedHash = NavGridHash.ComputeSha256(canonical);
            }
            catch (CryptographicException exception)
            {
                return Fail(NavGridLoadError.HashFailure, "SHA-256 failed: " + exception.Message);
            }

            if (!NavGridHash.FixedTimeEquals(storedHash, computedHash))
                return Fail(NavGridLoadError.HashMismatch, "NavGrid content SHA-256 does not match.");

            var asset =
                new NavGridAsset(version, mapId, width, height, cellSize, new Vector3(originX, originY, originZ),
                                 agentRadius, cellEncoding, storedHash, cells);

            return NavGridLoadResult.Ok(asset);
        }

        private static NavGridLoadResult Fail(NavGridLoadError error,
                                              string message) => NavGridLoadResult.Fail(error, message);
        private static bool IsFinite(float value) => !float.IsNaN(value) && !float.IsInfinity(value);

        private static bool TryReadUInt32(byte[] bytes, ref int cursor, out uint value)
        {
            value = 0;

            if (cursor < 0 || cursor > bytes.Length || bytes.Length - cursor < 4)
                return false;
            value = (uint)(bytes[cursor] | bytes[cursor + 1] << 8 | bytes[cursor + 2] << 16 | bytes[cursor + 3] << 24);
            cursor += 4;

            return true;
        }

        private static bool TryReadSingle(byte[] bytes, ref int cursor, out float value)
        {
            value = 0.0f;

            if (!TryReadUInt32(bytes, ref cursor, out uint bits))
                return false;
            value = BitConverter.Int32BitsToSingle(unchecked((int)bits));

            return true;
        }

        private static void WriteUInt32(Stream stream, uint value)
        {
            stream.WriteByte((byte)value);
            stream.WriteByte((byte)(value >> 8));
            stream.WriteByte((byte)(value >> 16));
            stream.WriteByte((byte)(value >> 24));
        }

        private static void WriteSingle(Stream stream, float value)
        {
            WriteUInt32(stream, unchecked((uint)BitConverter.SingleToInt32Bits(value)));
        }
    }
}
