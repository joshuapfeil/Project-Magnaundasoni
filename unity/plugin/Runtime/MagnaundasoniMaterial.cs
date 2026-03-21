// ============================================================================
// MagnaundasoniMaterial.cs – ScriptableObject for acoustic materials
// ============================================================================
using System;
using System.Collections.Generic;
using UnityEngine;

namespace Magnaundasoni
{
    [CreateAssetMenu(fileName = "NewAcousticMaterial",
        menuName = "Magnaundasoni/Acoustic Material", order = 100)]
    public class MagnaundasoniMaterial : ScriptableObject
    {
        // ----- Per-Band Properties (8 bands) -------------------------------
        [Header("Absorption (per band, 0-1)")]
        [SerializeField] private float[] _absorption = new float[]
            { 0.1f, 0.1f, 0.12f, 0.15f, 0.2f, 0.25f, 0.3f, 0.35f };

        [Header("Transmission (per band, 0-1)")]
        [SerializeField] private float[] _transmission = new float[]
            { 0.05f, 0.04f, 0.03f, 0.02f, 0.01f, 0.01f, 0.005f, 0.005f };

        [Header("Scattering (per band, 0-1)")]
        [SerializeField] private float[] _scattering = new float[]
            { 0.1f, 0.12f, 0.15f, 0.2f, 0.25f, 0.3f, 0.35f, 0.4f };

        [Header("Surface Properties")]
        [SerializeField] [Range(0f, 1f)] private float _roughness = 0.3f;

        [Tooltip("0=Thin, 1=Standard, 2=Thick")]
        [SerializeField] [Range(0, 2)] private int _thicknessClass = 1;

        [SerializeField] [Range(0f, 1f)] private float _leakage = 0f;

        [SerializeField] private string _categoryTag = "default";

        // ----- Public Accessors --------------------------------------------
        public float[] Absorption   => _absorption;
        public float[] Transmission => _transmission;
        public float[] Scattering   => _scattering;
        public float Roughness      => _roughness;
        public int ThicknessClass   => _thicknessClass;
        public float Leakage        => _leakage;
        public string CategoryTag   => _categoryTag;

        // ----- Native Registration State -----------------------------------
        public uint NativeMaterialID => _nativeID;
        public bool IsRegistered => _isRegistered;

        private uint _nativeID;
        private bool _isRegistered;

        // ----- Preset Names ------------------------------------------------
        public static readonly string[] PresetNames = new string[]
        {
            "general", "metal", "wood", "fabric", "rock", "dirt",
            "grass", "carpet", "glass", "concrete", "plaster", "water"
        };

        private static readonly Dictionary<string, MagMaterialDesc> PresetsByName =
            new Dictionary<string, MagMaterialDesc>(StringComparer.OrdinalIgnoreCase)
            {
                ["general"] = CreatePreset(
                    new[] { 0.10f, 0.12f, 0.14f, 0.16f, 0.18f, 0.20f, 0.22f, 0.24f },
                    new[] { 0.05f, 0.04f, 0.03f, 0.02f, 0.02f, 0.01f, 0.01f, 0.01f },
                    new[] { 0.10f, 0.12f, 0.15f, 0.18f, 0.22f, 0.25f, 0.28f, 0.30f },
                    0.5f, 1, 0.05f),
                ["metal"] = CreatePreset(
                    new[] { 0.01f, 0.01f, 0.02f, 0.02f, 0.03f, 0.04f, 0.05f, 0.05f },
                    new[] { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f },
                    new[] { 0.10f, 0.10f, 0.12f, 0.15f, 0.18f, 0.22f, 0.28f, 0.30f },
                    0.2f, 1, 0.00f),
                ["wood"] = CreatePreset(
                    new[] { 0.15f, 0.11f, 0.10f, 0.07f, 0.06f, 0.07f, 0.08f, 0.09f },
                    new[] { 0.04f, 0.03f, 0.02f, 0.02f, 0.01f, 0.01f, 0.01f, 0.01f },
                    new[] { 0.10f, 0.12f, 0.14f, 0.18f, 0.22f, 0.28f, 0.32f, 0.35f },
                    0.4f, 1, 0.02f),
                ["fabric"] = CreatePreset(
                    new[] { 0.03f, 0.04f, 0.11f, 0.17f, 0.24f, 0.35f, 0.45f, 0.50f },
                    new[] { 0.15f, 0.12f, 0.10f, 0.08f, 0.06f, 0.04f, 0.03f, 0.02f },
                    new[] { 0.10f, 0.15f, 0.20f, 0.30f, 0.40f, 0.50f, 0.55f, 0.60f },
                    0.8f, 0, 0.10f),
                ["rock"] = CreatePreset(
                    new[] { 0.02f, 0.02f, 0.03f, 0.04f, 0.05f, 0.05f, 0.05f, 0.05f },
                    new[] { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f },
                    new[] { 0.12f, 0.14f, 0.16f, 0.20f, 0.24f, 0.28f, 0.32f, 0.35f },
                    0.6f, 2, 0.00f),
                ["dirt"] = CreatePreset(
                    new[] { 0.15f, 0.25f, 0.40f, 0.55f, 0.60f, 0.60f, 0.60f, 0.60f },
                    new[] { 0.02f, 0.02f, 0.01f, 0.01f, 0.01f, 0.00f, 0.00f, 0.00f },
                    new[] { 0.20f, 0.30f, 0.40f, 0.50f, 0.55f, 0.60f, 0.60f, 0.60f },
                    0.9f, 2, 0.01f),
                ["grass"] = CreatePreset(
                    new[] { 0.11f, 0.26f, 0.60f, 0.69f, 0.92f, 0.99f, 0.99f, 0.99f },
                    new[] { 0.10f, 0.08f, 0.05f, 0.03f, 0.02f, 0.01f, 0.01f, 0.01f },
                    new[] { 0.30f, 0.40f, 0.50f, 0.60f, 0.70f, 0.80f, 0.85f, 0.90f },
                    0.95f, 0, 0.05f),
                ["carpet"] = CreatePreset(
                    new[] { 0.05f, 0.10f, 0.20f, 0.35f, 0.50f, 0.65f, 0.60f, 0.55f },
                    new[] { 0.10f, 0.08f, 0.06f, 0.04f, 0.03f, 0.02f, 0.02f, 0.01f },
                    new[] { 0.10f, 0.15f, 0.25f, 0.40f, 0.55f, 0.65f, 0.70f, 0.75f },
                    0.85f, 0, 0.08f),
                ["glass"] = CreatePreset(
                    new[] { 0.35f, 0.25f, 0.18f, 0.12f, 0.07f, 0.05f, 0.05f, 0.05f },
                    new[] { 0.08f, 0.06f, 0.04f, 0.03f, 0.02f, 0.02f, 0.01f, 0.01f },
                    new[] { 0.05f, 0.05f, 0.06f, 0.08f, 0.10f, 0.12f, 0.14f, 0.15f },
                    0.1f, 0, 0.03f),
                ["concrete"] = CreatePreset(
                    new[] { 0.01f, 0.01f, 0.02f, 0.02f, 0.02f, 0.03f, 0.04f, 0.04f },
                    new[] { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f },
                    new[] { 0.10f, 0.11f, 0.12f, 0.14f, 0.16f, 0.20f, 0.24f, 0.28f },
                    0.3f, 2, 0.00f),
                ["plaster"] = CreatePreset(
                    new[] { 0.01f, 0.02f, 0.02f, 0.03f, 0.04f, 0.05f, 0.05f, 0.05f },
                    new[] { 0.01f, 0.01f, 0.01f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f },
                    new[] { 0.10f, 0.12f, 0.14f, 0.16f, 0.20f, 0.24f, 0.28f, 0.30f },
                    0.35f, 1, 0.01f),
                ["water"] = CreatePreset(
                    new[] { 0.01f, 0.01f, 0.01f, 0.02f, 0.02f, 0.03f, 0.03f, 0.04f },
                    new[] { 0.90f, 0.85f, 0.80f, 0.70f, 0.60f, 0.50f, 0.40f, 0.30f },
                    new[] { 0.05f, 0.06f, 0.08f, 0.10f, 0.12f, 0.15f, 0.18f, 0.20f },
                    0.05f, 2, 0.50f)
            };

        // ----- Methods -----------------------------------------------------
        public void LoadPreset(string presetName)
        {
            EnsureArraySizes();

            if (PresetsByName.TryGetValue(presetName, out var managedPreset))
            {
                ApplyPreset(presetName, managedPreset);
                return;
            }

            try
            {
                ApplyPreset(presetName, MagAPI.MaterialGetPreset(presetName));
            }
            catch (Exception ex) when (
                ex is MagnaundasoniException ||
                ex is DllNotFoundException ||
                ex is EntryPointNotFoundException ||
                ex is BadImageFormatException)
            {
                Debug.LogWarning($"[Magnaundasoni] Failed to load preset '{presetName}': {ex.Message}");
            }
        }

        private void ApplyPreset(string presetName, MagMaterialDesc desc)
        {
            Array.Copy(desc.absorption, _absorption, MagConstants.MaxBands);
            Array.Copy(desc.transmission, _transmission, MagConstants.MaxBands);
            Array.Copy(desc.scattering, _scattering, MagConstants.MaxBands);
            _roughness = desc.roughness;
            _thicknessClass = (int)desc.thicknessClass;
            _leakage = desc.leakageHint;
            _categoryTag = presetName;
#if UNITY_EDITOR
            UnityEditor.EditorUtility.SetDirty(this);
#endif
        }

        private static MagMaterialDesc CreatePreset(
            float[] absorption,
            float[] transmission,
            float[] scattering,
            float roughness,
            uint thicknessClass,
            float leakageHint)
        {
            return new MagMaterialDesc
            {
                absorption = absorption,
                transmission = transmission,
                scattering = scattering,
                roughness = roughness,
                thicknessClass = thicknessClass,
                leakageHint = leakageHint,
                categoryTag = IntPtr.Zero
            };
        }

        public uint Register(IntPtr engineHandle)
        {
            if (_isRegistered) return _nativeID;

            EnsureArraySizes();

            var desc = new MagMaterialDesc
            {
                absorption   = (float[])_absorption.Clone(),
                transmission = (float[])_transmission.Clone(),
                scattering   = (float[])_scattering.Clone(),
                roughness    = _roughness,
                thicknessClass = (uint)_thicknessClass,
                leakageHint  = _leakage,
                categoryTag  = IntPtr.Zero
            };

            try
            {
                _nativeID = MagAPI.MaterialRegister(engineHandle, desc);
                _isRegistered = true;
                return _nativeID;
            }
            catch (MagnaundasoniException ex)
            {
                Debug.LogError($"[Magnaundasoni] Material register failed: {ex.Message}");
                return 0;
            }
        }

        public void ResetRegistration()
        {
            _isRegistered = false;
            _nativeID = 0;
        }

        private void EnsureArraySizes()
        {
            if (_absorption == null || _absorption.Length != MagConstants.MaxBands)
                _absorption = new float[MagConstants.MaxBands];
            if (_transmission == null || _transmission.Length != MagConstants.MaxBands)
                _transmission = new float[MagConstants.MaxBands];
            if (_scattering == null || _scattering.Length != MagConstants.MaxBands)
                _scattering = new float[MagConstants.MaxBands];
        }

        private void OnValidate()
        {
            EnsureArraySizes();
            for (int i = 0; i < MagConstants.MaxBands; i++)
            {
                _absorption[i]   = Mathf.Clamp01(_absorption[i]);
                _transmission[i] = Mathf.Clamp01(_transmission[i]);
                _scattering[i]   = Mathf.Clamp01(_scattering[i]);
            }
        }
    }
}
