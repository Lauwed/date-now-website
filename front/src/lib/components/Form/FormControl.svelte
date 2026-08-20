<script lang="ts">
	import Label from './Label.svelte';
	import Input from './Input.svelte';

	interface Props {
		label: string;
		id: string;
		required: boolean;
		type?: string;
		value?: unknown;
		[key: string]: unknown;
	}

	let { id, label, required, value = $bindable(), type = 'text', ...props }: Props = $props();
</script>

<div class="form-control">
	<Label forHTML={id}>{label}</Label>
	<Input {id} {required} {type} bind:value {...props} />
</div>

<style lang="scss">
	.form-control {
		display: flex;
		flex-direction: column;
		gap: 2px;
		width: 100%;
	}
</style>
