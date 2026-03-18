// ============================================================================
// MagnaundasoniMaterial.cs – ScriptableObject for acoustic materials
// ============================================================================
using System;
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
            "concrete", "brick", "drywall", "glass", "wood_floor",
            "carpet", "metal", "fabric", "plaster", "tile",
            "grass", "gravel", "water"
        };

        // ----- Methods -----------------------------------------------------
        public void LoadPreset(string presetName)
        {
            try
            {
                var desc = MagAPI.MaterialGetPreset(presetName);
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
            catch (MagnaundasoniException ex)
            {
                Debug.LogWarning($"[Magnaundasoni] Failed to load preset '{presetName}': {ex.Message}");
            }
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
