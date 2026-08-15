<script lang="ts">
	import type { Snippet } from 'svelte';
	import X from '@lucide/svelte/icons/x';
	import Button from './Button.svelte';

	interface Props {
		id: string;
		children: Snippet;
		tag?: 'h2' | 'h3' | 'h4';
		title?: string;
		customClass?: string;
	}

	let { id, children, tag = 'h3', title, customClass }: Props = $props();
</script>

<dialog {id} class={`modal${typeof customClass !== 'undefined' ? ` ${customClass}` : ''}`}>
	<div class="modal__header">
		{#if typeof title !== 'undefined'}
			<svelte:element this={tag} class="modal__title">
				{title}
			</svelte:element>
		{/if}

		<Button isIcon variant="ghost" title="Close the modal" commandfor={id} command="close"
			><X size={18} /></Button
		>
	</div>
	<div class="modal__content">{@render children()}</div>
</dialog>

<style lang="scss">
	@use './../styles/variables' as *;
	@use './../styles/mixins.scss' as *;

	.modal {
		padding: 0;
		min-width: 400px;
		max-width: 75vw;
		background: $white;
		border: 0px;
		border-radius: $border-radius;

		&::backdrop {
			background: rgba($black, 0.25);
		}

		&__header {
			padding: 12px 24px;
			display: flex;
			justify-content: end;
			align-items: center;
			gap: 24px;
			border-bottom: 2px solid $black;
		}

		&__title {
			flex: 1;
			margin: 0;
			word-break: break-all;
		}

		&__content {
			padding: 24px;
		}
	}
</style>
