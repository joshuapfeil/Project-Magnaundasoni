// ============================================================================
// MagnaundasoniNative.cs – P/Invoke bindings for the Magnaundasoni C ABI
// ============================================================================
using System;
using System.Runtime.InteropServices;

namespace Magnaundasoni
{
    // ------------------------------------------------------------------
    // Enums
    // ------------------------------------------------------------------
    public enum MagStatus : int
    {
        OK              =  0,
        Error           = -1,
        InvalidParam    = -2,
        OutOfMemory     = -3,
        NotInitialized  = -4
    }

    public enum MagQualityLevel : int
    {
        Low    = 0,
        Medium = 1,
        High   = 2,
        Ultra  = 3
    }

    public enum MagBackendType : int
    {
        Auto        = 0,
        SoftwareBVH = 1,
        DXR         = 2,
        VulkanRT    = 3
    }

    public enum MagDynamicFlag : uint
    {
        Static           = 0,
        QuasiStatic      = 1,
        DynamicImportant = 2,
        DynamicMinor     = 3
    }

    public enum MagImportance : uint
    {
        Low      = 0,
        Medium   = 1,
        High     = 2,
        Critical = 3
    }

    // ------------------------------------------------------------------
    // Constants
    // ------------------------------------------------------------------
    public static class MagConstants
    {
        public const int MaxBands = 8;
    }

    // ------------------------------------------------------------------
    // Structs – all LayoutKind.Sequential for correct marshalling
    // ------------------------------------------------------------------

    [StructLayout(LayoutKind.Sequential)]
    public struct MagEngineConfig
    {
        public MagQualityLevel quality;
        public MagBackendType  preferredBackend;
        public uint maxSources;
        public uint maxReflectionOrder;
        public uint maxDiffractionDepth;
        public uint raysPerSource;
        public uint threadCount;
        public float worldChunkSize;
        public uint effectiveBandCount;
    }

    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct MagGeometryDesc
    {
        public float*  vertices;
        public uint    vertexCount;
        public uint*   indices;
        public uint    indexCount;
        public uint    materialID;
        public uint    dynamicFlag;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct MagMaterialDesc
    {
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public float[] absorption;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public float[] transmission;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public float[] scattering;
        public float roughness;
        public uint  thicknessClass;
        public float leakageHint;
        public IntPtr categoryTag;

        public static MagMaterialDesc Create()
        {
            return new MagMaterialDesc
            {
                absorption   = new float[MagConstants.MaxBands],
                transmission = new float[MagConstants.MaxBands],
                scattering   = new float[MagConstants.MaxBands],
                roughness    = 0f,
                thicknessClass = 1,
                leakageHint  = 0f,
                categoryTag  = IntPtr.Zero
            };
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct MagSourceDesc
    {
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)]
        public float[] position;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)]
        public float[] direction;
        public float radius;
        public uint  importance;

        public static MagSourceDesc Create()
        {
            return new MagSourceDesc
            {
                position  = new float[3],
                direction = new float[] { 0f, 0f, 1f },
                radius    = 0.1f,
                importance = (uint)MagImportance.Medium
            };
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct MagListenerDesc
    {
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)]
        public float[] position;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)]
        public float[] forward;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)]
        public float[] up;

        public static MagListenerDesc Create()
        {
            return new MagListenerDesc
            {
                position = new float[3],
                forward  = new float[] { 0f, 0f, 1f },
                up       = new float[] { 0f, 1f, 0f }
            };
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct MagDirectComponent
    {
        public float delay;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)]
        public float[] direction;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public float[] perBandGain;
        public float occlusionLPF;
        public float confidence;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct MagReflectionTap
    {
        public uint tapID;
        public float delay;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)]
        public float[] direction;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public float[] perBandEnergy;
        public uint order;
        public float stability;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct MagDiffractionTap
    {
        public uint edgeID;
        public float delay;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)]
        public float[] direction;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public float[] perBandAttenuation;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct MagLateField
    {
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public float[] perBandDecay;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public float[] rt60;
        public float roomSizeEstimate;
        public float diffuseDirectionality;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct MagAcousticResult
    {
        public MagDirectComponent direct;
        public IntPtr reflections;
        public uint   reflectionCount;
        public IntPtr diffractions;
        public uint   diffractionCount;
        public MagLateField lateField;

        public MagReflectionTap[] GetReflections()
        {
            if (reflectionCount == 0 || reflections == IntPtr.Zero)
                return Array.Empty<MagReflectionTap>();

            var taps = new MagReflectionTap[reflectionCount];
            int size = Marshal.SizeOf<MagReflectionTap>();
            for (int i = 0; i < reflectionCount; i++)
            {
                IntPtr ptr = IntPtr.Add(reflections, i * size);
                taps[i] = Marshal.PtrToStructure<MagReflectionTap>(ptr);
            }
            return taps;
        }

        public MagDiffractionTap[] GetDiffractions()
        {
            if (diffractionCount == 0 || diffractions == IntPtr.Zero)
                return Array.Empty<MagDiffractionTap>();

            var taps = new MagDiffractionTap[diffractionCount];
            int size = Marshal.SizeOf<MagDiffractionTap>();
            for (int i = 0; i < diffractionCount; i++)
            {
                IntPtr ptr = IntPtr.Add(diffractions, i * size);
                taps[i] = Marshal.PtrToStructure<MagDiffractionTap>(ptr);
            }
            return taps;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct MagGlobalState
    {
        public MagQualityLevel activeQuality;
        public MagBackendType  backendUsed;
        public double timestamp;
        public uint   activeSourceCount;
        public float  cpuTimeMs;
    }

    // ------------------------------------------------------------------
    // Native library loader – resolves platform-specific library name
    // ------------------------------------------------------------------
    internal static class NativeLibraryName
    {
#if UNITY_EDITOR_WIN || UNITY_STANDALONE_WIN
        public const string Name = "magnaundasoni";
#elif UNITY_EDITOR_OSX || UNITY_STANDALONE_OSX || UNITY_IOS
        public const string Name = "magnaundasoni";
#else
        public const string Name = "magnaundasoni";
#endif
    }

    // ------------------------------------------------------------------
    // Raw P/Invoke declarations
    // ------------------------------------------------------------------
    public static class MagnaundasoniNative
    {
        private const string Lib = "magnaundasoni";

        // Engine lifecycle
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern MagStatus mag_engine_create(
            ref MagEngineConfig config, out IntPtr outEngine);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern MagStatus mag_engine_destroy(IntPtr engine);

        // Materials
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern MagStatus mag_material_register(
            IntPtr engine, ref MagMaterialDesc desc, out uint outID);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern MagStatus mag_material_get_preset(
            [MarshalAs(UnmanagedType.LPStr)] string presetName,
            out MagMaterialDesc outDesc);

        // Geometry
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe MagStatus mag_geometry_register(
            IntPtr engine, ref MagGeometryDesc desc, out uint outID);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern MagStatus mag_geometry_unregister(
            IntPtr engine, uint id);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern MagStatus mag_geometry_update_transform(
            IntPtr engine, uint id,
            [MarshalAs(UnmanagedType.LPArray, SizeConst = 16)] float[] transform4x4);

        // Sources
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern MagStatus mag_source_register(
            IntPtr engine, ref MagSourceDesc desc, out uint outID);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern MagStatus mag_source_unregister(
            IntPtr engine, uint id);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern MagStatus mag_source_update(
            IntPtr engine, uint id, ref MagSourceDesc desc);

        // Listeners
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern MagStatus mag_listener_register(
            IntPtr engine, ref MagListenerDesc desc, out uint outID);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern MagStatus mag_listener_unregister(
            IntPtr engine, uint id);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern MagStatus mag_listener_update(
            IntPtr engine, uint id, ref MagListenerDesc desc);

        // Simulation
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern MagStatus mag_update(
            IntPtr engine, float deltaTime);

        // Results
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern MagStatus mag_get_acoustic_result(
            IntPtr engine, uint sourceID, uint listenerID,
            out MagAcousticResult outResult);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern MagStatus mag_get_global_state(
            IntPtr engine, out MagGlobalState outState);

        // Quality
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern MagStatus mag_set_quality(
            IntPtr engine, MagQualityLevel level);

        // Debug
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern MagStatus mag_debug_get_ray_count(
            IntPtr engine, out uint outCount);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern MagStatus mag_debug_get_active_edges(
            IntPtr engine, out uint outCount);
    }

    // ------------------------------------------------------------------
    // Checked wrapper – throws MagnaundasoniException on failure
    // ------------------------------------------------------------------
    public class MagnaundasoniException : Exception
    {
        public MagStatus Status { get; }
        public MagnaundasoniException(MagStatus status, string api)
            : base($"Magnaundasoni API {api} failed with status {status}")
        {
            Status = status;
        }
    }

    public static class MagAPI
    {
        private static void Check(MagStatus s,
            [System.Runtime.CompilerServices.CallerMemberName] string caller = "")
        {
            if (s != MagStatus.OK)
                throw new MagnaundasoniException(s, caller);
        }

        public static IntPtr EngineCreate(MagEngineConfig config)
        {
            Check(MagnaundasoniNative.mag_engine_create(ref config, out IntPtr engine));
            return engine;
        }

        public static void EngineDestroy(IntPtr engine)
        {
            Check(MagnaundasoniNative.mag_engine_destroy(engine));
        }

        public static uint MaterialRegister(IntPtr engine, MagMaterialDesc desc)
        {
            Check(MagnaundasoniNative.mag_material_register(engine, ref desc, out uint id));
            return id;
        }

        public static MagMaterialDesc MaterialGetPreset(string presetName)
        {
            Check(MagnaundasoniNative.mag_material_get_preset(presetName, out MagMaterialDesc desc));
            return desc;
        }

        public static unsafe uint GeometryRegister(IntPtr engine, MagGeometryDesc desc)
        {
            Check(MagnaundasoniNative.mag_geometry_register(engine, ref desc, out uint id));
            return id;
        }

        public static void GeometryUnregister(IntPtr engine, uint id)
        {
            Check(MagnaundasoniNative.mag_geometry_unregister(engine, id));
        }

        public static void GeometryUpdateTransform(IntPtr engine, uint id, float[] transform4x4)
        {
            Check(MagnaundasoniNative.mag_geometry_update_transform(engine, id, transform4x4));
        }

        public static uint SourceRegister(IntPtr engine, MagSourceDesc desc)
        {
            Check(MagnaundasoniNative.mag_source_register(engine, ref desc, out uint id));
            return id;
        }

        public static void SourceUnregister(IntPtr engine, uint id)
        {
            Check(MagnaundasoniNative.mag_source_unregister(engine, id));
        }

        public static void SourceUpdate(IntPtr engine, uint id, MagSourceDesc desc)
        {
            Check(MagnaundasoniNative.mag_source_update(engine, id, ref desc));
        }

        public static uint ListenerRegister(IntPtr engine, MagListenerDesc desc)
        {
            Check(MagnaundasoniNative.mag_listener_register(engine, ref desc, out uint id));
            return id;
        }

        public static void ListenerUnregister(IntPtr engine, uint id)
        {
            Check(MagnaundasoniNative.mag_listener_unregister(engine, id));
        }

        public static void ListenerUpdate(IntPtr engine, uint id, MagListenerDesc desc)
        {
            Check(MagnaundasoniNative.mag_listener_update(engine, id, ref desc));
        }

        public static void Update(IntPtr engine, float deltaTime)
        {
            Check(MagnaundasoniNative.mag_update(engine, deltaTime));
        }

        public static MagAcousticResult GetAcousticResult(
            IntPtr engine, uint sourceID, uint listenerID)
        {
            Check(MagnaundasoniNative.mag_get_acoustic_result(
                engine, sourceID, listenerID, out MagAcousticResult result));
            return result;
        }

        public static MagGlobalState GetGlobalState(IntPtr engine)
        {
            Check(MagnaundasoniNative.mag_get_global_state(engine, out MagGlobalState state));
            return state;
        }

        public static void SetQuality(IntPtr engine, MagQualityLevel level)
        {
            Check(MagnaundasoniNative.mag_set_quality(engine, level));
        }

        public static uint DebugGetRayCount(IntPtr engine)
        {
            Check(MagnaundasoniNative.mag_debug_get_ray_count(engine, out uint count));
            return count;
        }

        public static uint DebugGetActiveEdges(IntPtr engine)
        {
            Check(MagnaundasoniNative.mag_debug_get_active_edges(engine, out uint count));
            return count;
        }
    }
}
