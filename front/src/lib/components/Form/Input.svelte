<script lang="ts">
	let { value = $bindable(), ...props } = $props();
</script>

<input class="input" bind:value {...props} />

<style lang="scss">
	@use './../../styles/variables' as *;

	.input {
		font-family: 'General Sans', sans-serif;
		border: 2px solid $black;
		border-radius: $border-radius-sm;
		padding: 4px 6px;
		font-size: 1em;
	}
</style>
